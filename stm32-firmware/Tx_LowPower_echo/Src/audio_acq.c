#include "audio_acq.h"
#include "main.h"
#include "AppAudioCfg.h"
#include "power_test_cfg.h"

#include <stddef.h>
#include <string.h>

#include "debug_uart.h"
#include "perf_counter.h"
#include "audio_ringbuf.h"

extern void AppClock_ApplyHSITrim(void);

extern const char *AppClock_GetAudioAdcClkSrcName(void);
extern uint32_t AppClock_GetAudioAdcClkHz(void);

#if (APP_AUDIO_ACQ_LOG_EN != 0)
  #define AUDIO_ACQ_LOG_PRINT(...)    Debug_Print(__VA_ARGS__)
  #define AUDIO_ACQ_LOG_PUTS(s)       Debug_PutString(s)
#else
  #define AUDIO_ACQ_LOG_PRINT(...)    do { } while (0)
  #define AUDIO_ACQ_LOG_PUTS(s)       do { } while (0)
#endif

#define AUDIO_ADC_TRIGGER_FS_HZ        (16000U)

#if (APP_ADC_MDF_CLK_SRC_SEL == APP_ADC_MDF_CLK_SRC_PLL)
  #define APP_ADC_CLKSEL            RCC_ADCDACCLKSOURCE_PLL2
  #define APP_ADC_MDF_CLK_SRC_NAME  "PLL2/PLL3"
#else
  #define APP_ADC_CLKSEL            RCC_ADCDACCLKSOURCE_MSIK
  #define APP_ADC_MDF_CLK_SRC_NAME  "MSIK"
#endif

ADC_HandleTypeDef hadc1;
DMA_NodeTypeDef Node_GPDMA1_Channel4;
DMA_QListTypeDef List_GPDMA1_Channel4;
DMA_HandleTypeDef handle_GPDMA1_Channel4;
LPTIM_HandleTypeDef hlptim1;

static int16_t s_dma_buffer[AUDIO_DMA_BUFFER_SAMPLES];
static uint32_t s_block_counter = 0U;
static uint8_t s_started = 0U;
static uint8_t s_hw_initialized = 0U;
static volatile uint32_t s_pending_committed_blocks = 0U;

static uint32_t s_rate_prev_cycles = 0U;
static uint32_t s_rate_prev_clk_hz = 0U;
static uint64_t s_rate_acc_time_ns = 0ULL;
static uint32_t s_rate_chunk_count = 0U;
static uint32_t s_rate_hz_x100 = 0U;

static volatile uint32_t s_lptim_cmp_count = 0U;
static volatile uint32_t s_dma_half_count = 0U;
static volatile uint32_t s_dma_full_count = 0U;
static volatile uint32_t s_dma_block_count = 0U;
static volatile uint32_t s_dma_sample_count = 0U;
static volatile uint32_t s_queue_overwrite_count = 0U;

static HAL_StatusTypeDef AudioAcq_EnableHSI(void);
static void MX_LPTIM1_Init_16kHz(void);
static void MX_ADC1_Init(void);
static void AudioAcq_RateMon_OnChunk(void);
static void AudioAcq_ResetDiag(void);

#define AUDIO_RATE_MON_BLOCK_WINDOW   (64U)

void AudioAcq_RateMon_NotifyClockChanged(uint32_t new_clk_hz);

static void AudioAcq_ResetDiag(void)
{
  __disable_irq();
  s_lptim_cmp_count = 0U;
  s_dma_half_count = 0U;
  s_dma_full_count = 0U;
  s_dma_block_count = 0U;
  s_dma_sample_count = 0U;
  s_queue_overwrite_count = 0U;
  s_pending_committed_blocks = 0U;
  __enable_irq();
}

/* Old HSI enable helper kept here only as reference in comments.
 * Use the trimmed version below. */
static HAL_StatusTypeDef AudioAcq_EnableHSI(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  HAL_StatusTypeDef st;

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

  st = HAL_RCC_OscConfig(&RCC_OscInitStruct);
  if (st != HAL_OK)
  {
    return st;
  }

  /* Re-apply trim after HSI is (re)configured, so LPTIM1 really sees the tested trim */
  AppClock_ApplyHSITrim();

  return HAL_OK;
}

static void MX_LPTIM1_Init_16kHz(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  LPTIM_OC_ConfigTypeDef sOCConfig = {0};
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (AudioAcq_EnableHSI() != HAL_OK)
  {
    AUDIO_ACQ_LOG_PRINT("[LPTIM1] enable HSI failed\r\n");
    Error_Handler();
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPTIM1;
  PeriphClkInit.Lptim1ClockSelection = RCC_LPTIM1CLKSOURCE_HSI;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    AUDIO_ACQ_LOG_PRINT("[LPTIM1] kernel clock select failed\r\n");
    Error_Handler();
  }

  __HAL_RCC_LPTIM1_CLK_ENABLE();

  __HAL_RCC_GPIOB_CLK_ENABLE();

#if (AUDIO_ACQ_LPTIM_PB2_EXPORT == AUDIO_ACQ_LPTIM_PB2_EXPORT_ON)
  /* Export raw LPTIM1_CH1 waveform to PB2 for oscilloscope measurement
     STM32U575: PB2 AF1 = LPTIM1_CH1
     NUCLEO-U575ZI-Q: PB2 is available on ST morpho CN12 pin 22 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF1_LPTIM1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  AUDIO_ACQ_LOG_PUTS("[LPTIM1] PB2 export ON (AF1)\r\n");
#else
  /* Power comparison mode: keep PB2 in analog when LPTIM trigger is used,
     so only HSI16 + LPTIM1 -> ADC are enabled without the GPIO export load. */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
#endif

  hlptim1.Instance = LPTIM1;
  hlptim1.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
  hlptim1.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV1;
  hlptim1.Init.UltraLowPowerClock.Polarity = LPTIM_CLOCKPOLARITY_RISING;
  hlptim1.Init.UltraLowPowerClock.SampleTime = LPTIM_CLOCKSAMPLETIME_DIRECTTRANSITION;
  hlptim1.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
  hlptim1.Init.Trigger.ActiveEdge = LPTIM_ACTIVEEDGE_RISING;
  hlptim1.Init.Trigger.SampleTime = LPTIM_TRIGSAMPLETIME_DIRECTTRANSITION;
  hlptim1.Init.Period = 999U; /* 16 MHz / (999 + 1) = 16 kHz */
//  hlptim1.Init.Period = 972U; /* tuned from PB2 scope result, target ~16.00 kHz */
  hlptim1.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
  hlptim1.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
  hlptim1.Init.Input1Source = LPTIM_INPUT1SOURCE_GPIO;
  hlptim1.Init.Input2Source = LPTIM_INPUT2SOURCE_GPIO;
  hlptim1.Init.RepetitionCounter = 0U;

  if (HAL_LPTIM_Init(&hlptim1) != HAL_OK)
  {
    AUDIO_ACQ_LOG_PRINT("[LPTIM1] HAL_LPTIM_Init failed\r\n");
    Error_Handler();
  }
  AUDIO_ACQ_LOG_PUTS("[LPTIM1] HAL_LPTIM_Init ok\r\n");

  sOCConfig.Pulse = 499U; /* 50% duty */
//  sOCConfig.Pulse = 486U; /* near 50% duty; exact 50% is impossible when ARR+1 = 973 */
  sOCConfig.OCPolarity = LPTIM_OCPOLARITY_HIGH;

  if (HAL_LPTIM_OC_ConfigChannel(&hlptim1, &sOCConfig, LPTIM_CHANNEL_1) != HAL_OK)
  {
    AUDIO_ACQ_LOG_PRINT("[LPTIM1] HAL_LPTIM_OC_ConfigChannel failed\r\n");
    Error_Handler();
  }
  AUDIO_ACQ_LOG_PUTS("[LPTIM1] OC ch1 config ok\r\n");

  HAL_NVIC_SetPriority(LPTIM1_IRQn, 5U, 0U);
}

void AudioAcq_Init(void)
{

  (void)memset(s_dma_buffer, 0, sizeof(s_dma_buffer));
  s_block_counter = 0U;
  s_pending_committed_blocks = 0U;
  s_started = 0U;

  if (s_hw_initialized == 0U)
  {
//    Debug_Print("[AUDIO-INIT] before MX_ADC1_Init\r\n");
//    MX_ADC1_Init();
//    Debug_Print("[AUDIO-INIT] after MX_ADC1_Init\r\n");
//
//    Debug_Print("[AUDIO-INIT] before MX_LPTIM1_Init_16kHz\r\n");
//    MX_LPTIM1_Init_16kHz();
//    Debug_Print("[AUDIO-INIT] after MX_LPTIM1_Init_16kHz\r\n");
#if (AUDIO_ACQ_LPTIM_PB2_EXPORT == AUDIO_ACQ_LPTIM_PB2_EXPORT_ON)
#else
#endif
//
////    Debug_Print("[ADC-DMA] source=%s\r\n", APP_ADC_MDF_CLK_SRC_NAME);
////    Debug_Print("[ADC-DMA] ADC kernel source = PLL2 (ADC only)\r\n");
////    Debug_Print("[ADC-DMA] ADC kernel source = MSIK (ADC only)\r\n");
//    Debug_Print("[ADC-DMA] ADC kernel source = %s (%lu Hz)\r\n",
//                AppClock_GetAudioAdcClkSrcName(),
//                (unsigned long)AppClock_GetAudioAdcClkHz());
//    s_hw_initialized = 1U;

	  MX_ADC1_Init();

	  #if (AUDIO_ACQ_MODE_SEL == AUDIO_ACQ_MODE_LPTIM_TRIGGER)
	    MX_LPTIM1_Init_16kHz();
#if (AUDIO_ACQ_LPTIM_PB2_EXPORT == AUDIO_ACQ_LPTIM_PB2_EXPORT_ON)
#else
#endif
	  #else
	    /* Continuous ADC test: do not enable HSI16 or LPTIM1 */
	    __HAL_RCC_LPTIM1_CLK_DISABLE();

	    /* PB2 was used for LPTIM1_CH1 export before; set it to analog to avoid leakage */
	    __HAL_RCC_GPIOB_CLK_ENABLE();
	    {
	      GPIO_InitTypeDef GPIO_InitStruct = {0};
	      GPIO_InitStruct.Pin = GPIO_PIN_2;
	      GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	      GPIO_InitStruct.Pull = GPIO_NOPULL;
	      HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	    }

	  #endif

	  s_hw_initialized = 1U;
  }
}

HAL_StatusTypeDef AudioAcq_Start(void)
{
  if (s_hw_initialized == 0U)
  {
    return HAL_ERROR;
  }

  (void)memset(s_dma_buffer, 0, sizeof(s_dma_buffer));
  s_block_counter = 0U;
  s_pending_committed_blocks = 0U;

//  AudioAcq_ResetMeasuredSampleRate();
//
//  AudioAcq_ResetDiag();
//
//  Debug_Print("[AUDIO-START] before DMA irq\r\n");
//  HAL_NVIC_ClearPendingIRQ(GPDMA1_Channel4_IRQn);
//  HAL_NVIC_EnableIRQ(GPDMA1_Channel4_IRQn);
//
//  Debug_Print("[AUDIO-START] before HAL_ADC_Start_DMA\r\n");
//  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_dma_buffer, AUDIO_DMA_BUFFER_SAMPLES) != HAL_OK)
//  {
//    HAL_NVIC_DisableIRQ(GPDMA1_Channel4_IRQn);
//    Debug_Print("[ADC-DMA] HAL_ADC_Start_DMA failed, adc_err=0x%08lx dma_err=0x%08lx\r\n",
//                (unsigned long)hadc1.ErrorCode,
//                (unsigned long)handle_GPDMA1_Channel4.ErrorCode);
//    return HAL_ERROR;
//  }
//  Debug_Print("[AUDIO-START] after HAL_ADC_Start_DMA\r\n");
//
//  s_started = 1U;
//
//  Debug_Print("[AUDIO-START] before HAL_LPTIM_PWM_Start\r\n");
//  if (HAL_LPTIM_PWM_Start(&hlptim1, LPTIM_CHANNEL_1) != HAL_OK)
//  {
//    (void)HAL_ADC_Stop_DMA(&hadc1);
//    s_started = 0U;
//    HAL_NVIC_DisableIRQ(GPDMA1_Channel4_IRQn);
////    Debug_Print("[LPTIM1] HAL_LPTIM_PWM_Start failed, lptim_err=0x%08lx\r\n",
////                (unsigned long)hlptim1.ErrorCode);
//    AUDIO_ACQ_LOG_PRINT("[LPTIM1] HAL_LPTIM_PWM_Start failed\r\n");
//    return HAL_ERROR;
//  }
////  Debug_Print("[AUDIO-START] after HAL_LPTIM_PWM_Start\r\n");
////
////  Debug_Print("[ADC-DMA] HAL_ADC_Start_DMA ok\r\n");
////  Debug_Print("[LPTIM1] trigger=%lu Hz\r\n", (unsigned long)AUDIO_ADC_TRIGGER_FS_HZ);
////  return HAL_OK;
//
//  Debug_Print("[AUDIO-START] after HAL_LPTIM_PWM_Start\r\n");
//
////  __HAL_LPTIM_CLEAR_FLAG(&hlptim1, LPTIM_FLAG_CC1);
////  __HAL_LPTIM_ENABLE_IT(&hlptim1, LPTIM_IT_CC1);
////  HAL_NVIC_ClearPendingIRQ(LPTIM1_IRQn);
////  HAL_NVIC_EnableIRQ(LPTIM1_IRQn);
////  Debug_Print("[LPTIM1] CC1 irq enabled\r\n");
//
//  Debug_Print("[ADC-DMA] HAL_ADC_Start_DMA ok\r\n");
//  Debug_Print("[LPTIM1] trigger=%lu Hz\r\n", (unsigned long)AUDIO_ADC_TRIGGER_FS_HZ);
//  return HAL_OK;

  AudioAcq_ResetMeasuredSampleRate();
  AudioAcq_ResetDiag();

  #if (AUDIO_ACQ_MODE_SEL == AUDIO_ACQ_MODE_LPTIM_TRIGGER)
    __HAL_LPTIM_CLEAR_FLAG(&hlptim1, LPTIM_FLAG_ARRM);
    __HAL_LPTIM_ENABLE_IT(&hlptim1, LPTIM_IT_ARRM);
    HAL_NVIC_ClearPendingIRQ(LPTIM1_IRQn);
    HAL_NVIC_EnableIRQ(LPTIM1_IRQn);
  #endif

  HAL_NVIC_ClearPendingIRQ(GPDMA1_Channel4_IRQn);
  HAL_NVIC_EnableIRQ(GPDMA1_Channel4_IRQn);

  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_dma_buffer, AUDIO_DMA_BUFFER_SAMPLES) != HAL_OK)
  {
  #if (AUDIO_ACQ_MODE_SEL == AUDIO_ACQ_MODE_LPTIM_TRIGGER)
    __HAL_LPTIM_DISABLE_IT(&hlptim1, LPTIM_IT_ARRM);
    HAL_NVIC_DisableIRQ(LPTIM1_IRQn);
  #endif
    HAL_NVIC_DisableIRQ(GPDMA1_Channel4_IRQn);
    AUDIO_ACQ_LOG_PRINT("[ADC-DMA] HAL_ADC_Start_DMA failed\r\n");
    return HAL_ERROR;
  }

  s_started = 1U;

  #if (AUDIO_ACQ_MODE_SEL == AUDIO_ACQ_MODE_LPTIM_TRIGGER)
    if (HAL_LPTIM_PWM_Start(&hlptim1, LPTIM_CHANNEL_1) != HAL_OK)
    {
      (void)HAL_ADC_Stop_DMA(&hadc1);
      s_started = 0U;
      __HAL_LPTIM_DISABLE_IT(&hlptim1, LPTIM_IT_ARRM);
      HAL_NVIC_DisableIRQ(LPTIM1_IRQn);
      HAL_NVIC_DisableIRQ(GPDMA1_Channel4_IRQn);
      AUDIO_ACQ_LOG_PRINT("[LPTIM1] HAL_LPTIM_PWM_Start failed\r\n");
      return HAL_ERROR;
    }
    AUDIO_ACQ_LOG_PUTS("[LPTIM1] PWM start ok (PB2 should toggle)\r\n");

  #else
  #endif

  return HAL_OK;

}

//HAL_StatusTypeDef AudioAcq_Start(void)
//{
//  if (s_hw_initialized == 0U)
//  {
//    return HAL_ERROR;
//  }
//
//  (void)memset(s_dma_buffer, 0, sizeof(s_dma_buffer));
//  s_q_head = 0U;
//  s_q_tail = 0U;
//  s_q_count = 0U;
//  s_block_counter = 0U;
//
////  AudioAcq_ResetMeasuredSampleRate();
////  AudioAcq_ResetDiag();
////
////  __HAL_LPTIM_CLEAR_FLAG(&hlptim1, LPTIM_FLAG_CC1);
////  __HAL_LPTIM_ENABLE_IT(&hlptim1, LPTIM_IT_CC1);
////  HAL_NVIC_ClearPendingIRQ(LPTIM1_IRQn);
////  HAL_NVIC_EnableIRQ(LPTIM1_IRQn);
////
////  HAL_NVIC_ClearPendingIRQ(GPDMA1_Channel4_IRQn);
////  HAL_NVIC_EnableIRQ(GPDMA1_Channel4_IRQn);
//  AudioAcq_ResetMeasuredSampleRate();
//
//  s_lptim_cmp_count = 0U;
//  s_dma_half_count = 0U;
//  s_dma_full_count = 0U;
//  s_dma_block_count = 0U;
//  s_dma_sample_count = 0U;
//  s_queue_overwrite_count = 0U;
//
//  HAL_NVIC_ClearPendingIRQ(LPTIM1_IRQn);
//  HAL_NVIC_EnableIRQ(LPTIM1_IRQn);
//
//  HAL_NVIC_ClearPendingIRQ(GPDMA1_Channel4_IRQn);
//  HAL_NVIC_EnableIRQ(GPDMA1_Channel4_IRQn);
//
//
//
//  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_dma_buffer, AUDIO_DMA_BUFFER_SAMPLES) != HAL_OK)
//  {
//    __HAL_LPTIM_DISABLE_IT(&hlptim1, LPTIM_IT_ARRM);
//    HAL_NVIC_DisableIRQ(LPTIM1_IRQn);
//    HAL_NVIC_DisableIRQ(GPDMA1_Channel4_IRQn);
//    AUDIO_ACQ_LOG_PRINT("[ADC-DMA] HAL_ADC_Start_DMA failed\r\n");
//    return HAL_ERROR;
//  }
//
//  s_started = 1U;
//
//  if (HAL_LPTIM_PWM_Start(&hlptim1, LPTIM_CHANNEL_1) != HAL_OK)
//  {
//    (void)HAL_ADC_Stop_DMA(&hadc1);
//    s_started = 0U;
//    __HAL_LPTIM_DISABLE_IT(&hlptim1, LPTIM_IT_ARRM);
//    HAL_NVIC_DisableIRQ(LPTIM1_IRQn);
//    HAL_NVIC_DisableIRQ(GPDMA1_Channel4_IRQn);
//    AUDIO_ACQ_LOG_PRINT("[LPTIM1] HAL_LPTIM_PWM_Start failed\r\n");
//    return HAL_ERROR;
//  }
//  __HAL_LPTIM_ENABLE_IT(&hlptim1, LPTIM_IT_CC1);
//
//  Debug_Print("[ADC-DMA] HAL_ADC_Start_DMA ok\r\n");
//  Debug_Print("[LPTIM1] trigger=%lu Hz\r\n", (unsigned long)AUDIO_ADC_TRIGGER_FS_HZ);
//  return HAL_OK;
//}

//void AudioAcq_Stop(void)
//{
//  __HAL_LPTIM_DISABLE_IT(&hlptim1, LPTIM_IT_ARRM);
//
//  if (s_started != 0U)
//  {
//    (void)HAL_LPTIM_PWM_Stop(&hlptim1, LPTIM_CHANNEL_1);
//    (void)HAL_ADC_Stop_DMA(&hadc1);
//  }
//
//  HAL_NVIC_DisableIRQ(LPTIM1_IRQn);
//  HAL_NVIC_DisableIRQ(GPDMA1_Channel4_IRQn);
//
//  s_started = 0U;
//  s_q_head = 0U;
//  s_q_tail = 0U;
//  s_q_count = 0U;
//}

//void AudioAcq_Stop(void)
//{
////  if (s_started != 0U)
////  {
////    (void)HAL_LPTIM_PWM_Stop(&hlptim1, LPTIM_CHANNEL_1);
////    (void)HAL_ADC_Stop_DMA(&hadc1);
////  }
////
////  HAL_NVIC_DisableIRQ(GPDMA1_Channel4_IRQn);
////
////  s_started = 0U;
////  s_q_head = 0U;
////  s_q_tail = 0U;
////  s_q_count = 0U;
//#if (AUDIO_ACQ_MODE_SEL == AUDIO_ACQ_MODE_LPTIM_TRIGGER)
//  __HAL_LPTIM_DISABLE_IT(&hlptim1, LPTIM_IT_ARRM);
//#endif
//
//if (s_started != 0U)
//{
//#if (AUDIO_ACQ_MODE_SEL == AUDIO_ACQ_MODE_LPTIM_TRIGGER)
//  (void)HAL_LPTIM_PWM_Stop(&hlptim1, LPTIM_CHANNEL_1);
//#endif
//  (void)HAL_ADC_Stop_DMA(&hadc1);
//}
//
//#if (AUDIO_ACQ_MODE_SEL == AUDIO_ACQ_MODE_LPTIM_TRIGGER)
//  HAL_NVIC_DisableIRQ(LPTIM1_IRQn);
//#endif
//HAL_NVIC_DisableIRQ(GPDMA1_Channel4_IRQn);
//
//	s_started = 0U;
//	s_q_head = 0U;
//	s_q_tail = 0U;
//	s_q_count = 0U;
//}

void AudioAcq_Stop(void)
{
  HAL_NVIC_DisableIRQ(LPTIM1_IRQn);

#if (AUDIO_ACQ_MODE_SEL == AUDIO_ACQ_MODE_LPTIM_TRIGGER)
  __HAL_LPTIM_DISABLE_IT(&hlptim1, LPTIM_IT_ARRM);
#endif

  if (s_started != 0U)
  {
#if (AUDIO_ACQ_MODE_SEL == AUDIO_ACQ_MODE_LPTIM_TRIGGER)
    (void)HAL_LPTIM_PWM_Stop(&hlptim1, LPTIM_CHANNEL_1);
#endif
    (void)HAL_ADC_Stop_DMA(&hadc1);
  }

  HAL_NVIC_DisableIRQ(GPDMA1_Channel4_IRQn);

  s_started = 0U;
  s_pending_committed_blocks = 0U;
}

//void AudioAcq_Stop(void)
//{
//  __HAL_LPTIM_DISABLE_IT(&hlptim1, LPTIM_IT_ARRM);
//  HAL_NVIC_DisableIRQ(LPTIM1_IRQn);
//
//  if (s_started != 0U)
//  {
//    (void)HAL_LPTIM_PWM_Stop(&hlptim1, LPTIM_CHANNEL_1);
//    (void)HAL_ADC_Stop_DMA(&hadc1);
//  }
//
//  HAL_NVIC_DisableIRQ(GPDMA1_Channel4_IRQn);
//
//  s_started = 0U;
//  s_q_head = 0U;
//  s_q_tail = 0U;
//  s_q_count = 0U;
//}

uint8_t AudioAcq_IsStarted(void)
{
  return s_started;
}

int16_t *AudioAcq_GetDmaBuffer(void)
{
  return s_dma_buffer;
}

uint32_t AudioAcq_GetDmaBufferSampleCount(void)
{
  return AUDIO_DMA_BUFFER_SAMPLES;
}

//void AudioAcq_OnHalfTransfer(void)
//{
//  if (s_started != 0U)
//  {
//    s_dma_half_count++;
//    s_dma_block_count++;
//    s_dma_sample_count += AUDIO_DMA_CHUNK_SAMPLES;
//
//    HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_3);
//    AudioAcq_RateMon_OnChunk();
//    AudioAcq_QueueBlock(&s_dma_buffer[0], AUDIO_DMA_CHUNK_SAMPLES);
//  }
//}

static void AudioAcq_CommitChunkToRing(const int16_t *ptr)
{
#if (APP_AUDIO_STAGE_SEL != APP_AUDIO_STAGE_RAW_DMA_ONLY)
  (void)AudioRingBuf_PushSamples(ptr, AUDIO_DMA_CHUNK_SAMPLES);
#endif
  s_pending_committed_blocks++;
  AudioAcq_BlockCommittedFromISR();
}

void AudioAcq_OnHalfTransfer(void)
{
  if (s_started != 0U)
  {
    s_dma_half_count++;
    s_dma_block_count++;
    s_dma_sample_count += AUDIO_DMA_CHUNK_SAMPLES;
    AudioAcq_RateMon_OnChunk();
    AudioAcq_CommitChunkToRing(&s_dma_buffer[0]);
  }
}

void AudioAcq_OnTransferComplete(void)
{
  if (s_started != 0U)
  {
    s_dma_full_count++;
    s_dma_block_count++;
    s_dma_sample_count += AUDIO_DMA_CHUNK_SAMPLES;
    AudioAcq_RateMon_OnChunk();
    AudioAcq_CommitChunkToRing(&s_dma_buffer[AUDIO_DMA_CHUNK_SAMPLES]);
  }
}

HAL_StatusTypeDef AudioAcq_FeedBlock(const int16_t *samples, uint16_t sample_count)
{
  if ((samples == NULL) || (sample_count == 0U) || (sample_count > AUDIO_DMA_CHUNK_SAMPLES))
  {
    return HAL_ERROR;
  }

  if (s_started == 0U)
  {
    return HAL_BUSY;
  }

#if (APP_AUDIO_STAGE_SEL != APP_AUDIO_STAGE_RAW_DMA_ONLY)
  (void)AudioRingBuf_PushSamples(samples, sample_count);
#endif

  __disable_irq();
  s_block_counter++;
  s_pending_committed_blocks++;
  __enable_irq();
  return HAL_OK;
}

uint8_t AudioAcq_GetNextBlock(AudioAcq_Block_t *out_block)
{
  (void)out_block;
  return 0U;
}

uint32_t AudioAcq_TakeCommittedBlockCount(void)
{
  uint32_t count;

  __disable_irq();
  count = s_pending_committed_blocks;
  s_pending_committed_blocks = 0U;
  __enable_irq();

  return count;
}

uint32_t AudioAcq_GetMeasuredSampleRate_x100(void)
{
  return s_rate_hz_x100;
}

void AudioAcq_ResetMeasuredSampleRate(void)
{
  s_rate_prev_cycles = PerfCounter_GetCycles();
  s_rate_prev_clk_hz = SystemCoreClock;
  s_rate_acc_time_ns = 0ULL;
  s_rate_chunk_count = 0U;
  s_rate_hz_x100 = 0U;
}

void AudioAcq_RateMon_NotifyClockChanged(uint32_t new_clk_hz)
{
  uint32_t now_cycles = PerfCounter_GetCycles();
  uint32_t diff_cycles = now_cycles - s_rate_prev_cycles;

  if ((s_rate_prev_clk_hz != 0U) && (diff_cycles != 0U))
  {
    s_rate_acc_time_ns += ((uint64_t)diff_cycles * 1000000000ULL) /
                          (uint64_t)s_rate_prev_clk_hz;
  }

  s_rate_prev_cycles = now_cycles;
  s_rate_prev_clk_hz = new_clk_hz;
}


static void AudioAcq_RateMon_OnChunk(void)
{
  uint32_t now_cycles = PerfCounter_GetCycles();
  uint32_t diff_cycles = now_cycles - s_rate_prev_cycles;

  s_rate_prev_cycles = now_cycles;

  if ((s_rate_prev_clk_hz != 0U) && (diff_cycles != 0U))
  {
    s_rate_acc_time_ns += ((uint64_t)diff_cycles * 1000000000ULL) /
                          (uint64_t)s_rate_prev_clk_hz;
  }

  s_rate_chunk_count++;

  if ((s_rate_chunk_count >= AUDIO_RATE_MON_BLOCK_WINDOW) && (s_rate_acc_time_ns != 0ULL))
  {
    uint64_t total_samples = (uint64_t)s_rate_chunk_count * (uint64_t)AUDIO_DMA_CHUNK_SAMPLES;
    s_rate_hz_x100 = (uint32_t)((total_samples * 100ULL * 1000000000ULL + (s_rate_acc_time_ns / 2ULL)) /
                                s_rate_acc_time_ns);

    s_rate_chunk_count = 0U;
    s_rate_acc_time_ns = 0ULL;
  }
}

static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};


  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.GainCompensation = 0;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
//  hadc1.Init.DiscontinuousConvMode = DISABLE;
//  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_LPTIM1_CH1;
//  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
#if (AUDIO_ACQ_MODE_SEL == AUDIO_ACQ_MODE_ADC_CONTINUOUS)
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
#else
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_LPTIM1_CH1;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
#endif
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc1.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
  hadc1.Init.OversamplingMode = DISABLE;

  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    AUDIO_ACQ_LOG_PRINT("[ADC] HAL_ADC_Init failed\r\n");
    Error_Handler();
  }

  sConfig.Channel = ADC_CHANNEL_7;
  sConfig.Rank = ADC_REGULAR_RANK_1;
//  sConfig.SamplingTime = ADC_SAMPLETIME_5CYCLE;
  sConfig.SamplingTime = ADC_SAMPLETIME_12CYCLES;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
//  sConfig.OffsetNumber = ADC_OFFSET_1;
//  sConfig.Offset = AUDIO_ADC_MIDPOINT_OFFSET;
  sConfig.OffsetNumber = ADC_OFFSET_1;
  sConfig.Offset = 2053;


  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    AUDIO_ACQ_LOG_PRINT("[ADC] HAL_ADC_ConfigChannel failed\r\n");
    Error_Handler();
  }

  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
  {
    AUDIO_ACQ_LOG_PRINT("[ADC] HAL_ADCEx_Calibration_Start failed\r\n");
    Error_Handler();
  }
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  if ((hadc != NULL) && (hadc->Instance == ADC1))
  {
    AudioAcq_OnHalfTransfer();
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if ((hadc != NULL) && (hadc->Instance == ADC1))
  {
    AudioAcq_OnTransferComplete();
  }
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *adcHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  DMA_NodeConfTypeDef NodeConfig = {0};

  if (adcHandle->Instance == ADC1)
  {
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_EnableVddA();

    __HAL_RCC_ADC12_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPDMA1_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    NodeConfig.NodeType = DMA_GPDMA_LINEAR_NODE;
    NodeConfig.Init.Request = GPDMA1_REQUEST_ADC1;
    NodeConfig.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
    NodeConfig.Init.Direction = DMA_PERIPH_TO_MEMORY;
    NodeConfig.Init.SrcInc = DMA_SINC_FIXED;
    NodeConfig.Init.DestInc = DMA_DINC_INCREMENTED;
    NodeConfig.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_HALFWORD;
    NodeConfig.Init.DestDataWidth = DMA_DEST_DATAWIDTH_HALFWORD;
    NodeConfig.Init.SrcBurstLength = 1;
    NodeConfig.Init.DestBurstLength = 1;
    NodeConfig.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT0;
    NodeConfig.Init.Mode = DMA_NORMAL;
    NodeConfig.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
    NodeConfig.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
    NodeConfig.DataHandlingConfig.DataAlignment = DMA_DATA_UNPACK;

    if (HAL_DMAEx_List_BuildNode(&NodeConfig, &Node_GPDMA1_Channel4) != HAL_OK)
    {
      Error_Handler();
    }
    if (HAL_DMAEx_List_InsertNode(&List_GPDMA1_Channel4, NULL, &Node_GPDMA1_Channel4) != HAL_OK)
    {
      Error_Handler();
    }
    if (HAL_DMAEx_List_SetCircularMode(&List_GPDMA1_Channel4) != HAL_OK)
    {
      Error_Handler();
    }

    handle_GPDMA1_Channel4.Instance = GPDMA1_Channel4;
    handle_GPDMA1_Channel4.InitLinkedList.Priority = DMA_HIGH_PRIORITY;
    handle_GPDMA1_Channel4.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
    handle_GPDMA1_Channel4.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
    handle_GPDMA1_Channel4.InitLinkedList.TransferEventMode = DMA_TCEM_LAST_LL_ITEM_TRANSFER;
    handle_GPDMA1_Channel4.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;

    if (HAL_DMAEx_List_Init(&handle_GPDMA1_Channel4) != HAL_OK)
    {
      Error_Handler();
    }
    if (HAL_DMAEx_List_LinkQ(&handle_GPDMA1_Channel4, &List_GPDMA1_Channel4) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(adcHandle, DMA_Handle, handle_GPDMA1_Channel4);

    if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel4, DMA_CHANNEL_NPRIV) != HAL_OK)
    {
      Error_Handler();
    }

    HAL_NVIC_SetPriority(GPDMA1_Channel4_IRQn, 5U, 0U);
  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef *adcHandle)
{
  if (adcHandle->Instance == ADC1)
  {
    HAL_DMA_DeInit(adcHandle->DMA_Handle);
    __HAL_RCC_ADC12_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2);
  }
}

void AudioAcq_GetDiag(AudioAcq_Diag_t *out_diag)
{
  if (out_diag == NULL)
  {
    return;
  }

  __disable_irq();
  out_diag->lptim_cc1_count = s_lptim_cmp_count;
  out_diag->dma_half_count = s_dma_half_count;
  out_diag->dma_full_count = s_dma_full_count;
  out_diag->dma_block_count = s_dma_block_count;
  out_diag->dma_sample_count = s_dma_sample_count;
  out_diag->queue_overwrite_count = s_queue_overwrite_count;
  out_diag->queue_depth = s_pending_committed_blocks;
  __enable_irq();
}


void AudioAcq_LptimPeriodIrqHandler(void)
{
#if (AUDIO_ACQ_MODE_SEL == AUDIO_ACQ_MODE_LPTIM_TRIGGER)
  if (__HAL_LPTIM_GET_IT_SOURCE(&hlptim1, LPTIM_IT_ARRM) != RESET)
  {
    if (__HAL_LPTIM_GET_FLAG(&hlptim1, LPTIM_FLAG_ARRM) != RESET)
    {
      __HAL_LPTIM_CLEAR_FLAG(&hlptim1, LPTIM_FLAG_ARRM);
      s_lptim_cmp_count++;
    }
  }
#endif
}

void HAL_LPTIM_CompareMatchCallback(LPTIM_HandleTypeDef *hlptim)
{
  (void)hlptim;
}

void HAL_LPTIM_AutoReloadMatchCallback(LPTIM_HandleTypeDef *hlptim)
{
  (void)hlptim;
}
