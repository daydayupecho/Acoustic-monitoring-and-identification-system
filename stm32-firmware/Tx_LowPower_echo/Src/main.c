/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ThreadX/Tx_LowPower/Src/main.c
  * @author  MCD Application Team
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "debug_uart.h"
#include "audio_acq.h"
#include "power_test_cfg.h"
extern HAL_StatusTypeDef AppClock_ConfigAudioClockSelected(void);
extern void AppClock_ApplyHSITrim(void);
extern const char *AppClock_GetAudioAdcClkSrcName(void);
extern uint32_t AppClock_GetAudioAdcClkHz(void);

#if (APP_MAIN_BOOT_LOG_EN != 0)
  #define APP_MAIN_LOG_PRINT(...)  Debug_Print(__VA_ARGS__)
#else
  #define APP_MAIN_LOG_PRINT(...)  do { } while (0)
#endif
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint32_t s_app_sysclk_mhz = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void SystemPower_Config(void);
static void MX_ICACHE_Init(void);
static void App_SystemClock_ConfigMHz(uint32_t mhz);
static void App_ThreadXSysTick_Update(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void App_TimingPulse_DelayUs(uint32_t us)
{
#if (APP_TIMING_PULSE_ENABLE != 0)
  volatile uint32_t cycles;
  volatile uint32_t i;

  cycles = (SystemCoreClock / 1000000U) * us;
  if (cycles == 0U)
  {
    cycles = 1U;
  }

  for (i = 0U; i < cycles; i++)
  {
    __NOP();
  }
#else
  (void)us;
#endif
}

void App_TimingPulse_Event(uint32_t event_id)
{
#if (APP_TIMING_PULSE_ENABLE != 0)
  uint32_t i;

  for (i = 0U; i < event_id; i++)
  {
    HAL_GPIO_WritePin(APP_TIMING_PULSE_GPIO_Port, APP_TIMING_PULSE_Pin, GPIO_PIN_SET);
    App_TimingPulse_DelayUs(APP_TIMING_PULSE_WIDTH_US);
    HAL_GPIO_WritePin(APP_TIMING_PULSE_GPIO_Port, APP_TIMING_PULSE_Pin, GPIO_PIN_RESET);
    if ((i + 1U) < event_id)
    {
      App_TimingPulse_DelayUs(APP_TIMING_PULSE_GAP_US);
    }
  }
#endif
}

void App_StageMark_Set(GPIO_PinState state)
{
#if (APP_STAGE_MARK_ENABLE != 0)
  HAL_GPIO_WritePin(APP_STAGE_MARK_PULSE_GPIO_Port, APP_STAGE_MARK_PULSE_Pin, state);
#else
  (void)state;
#endif
}

static void App_ThreadXSysTick_Update(void)
{
  /* tx_initialize_low_level.S uses 100 ticks/s. Refresh SysTick after runtime clock changes. */
  SysTick->LOAD = (SystemCoreClock / 100U) - 1U;
  SysTick->VAL = 0U;
}

static void App_SystemClock_ConfigMHz(uint32_t mhz)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  if (mhz >= 160U)
  {
    /* High-speed profile:
     * MSI stays at 4 MHz and feeds PLL (4 * 80 / 2 = 160 MHz).
     * This matches the original working project clock tree. */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_4;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
    RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 80;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_0;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                                |RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
      Error_Handler();
    }

    s_app_sysclk_mhz = 160U;
  }
  else
  {
    uint32_t msi_range = RCC_MSIRANGE_2; /* 16 MHz direct MSI */
    uint32_t flash_latency = FLASH_LATENCY_1;
    uint32_t target_mhz = 16U;

    if (mhz <= 2U)
    {
      msi_range = RCC_MSIRANGE_5; /* 2 MHz direct MSI */
      flash_latency = FLASH_LATENCY_0;
      target_mhz = 2U;
    }

    /* Low-speed profile:
     * Do NOT use low-VCO PLL recipes here. Switch SYSCLK to MSI direct and
     * keep PLL off so A-mode baseline can run at 2 MHz safely. */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                                |RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
      Error_Handler();
    }

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.MSIClockRange = msi_range;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_OFF;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                                |RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, flash_latency) != HAL_OK)
    {
      Error_Handler();
    }

    s_app_sysclk_mhz = target_mhz;
  }

  SystemCoreClockUpdate();
  App_ThreadXSysTick_Update();
}

void App_SystemClock_SetLow(void)
{
#if (APP_SYSCLK_DYNAMIC_INFER_BOOST != 0)
  if (s_app_sysclk_mhz != APP_SYSCLK_LOW_MHZ)
  {
    App_SystemClock_ConfigMHz(APP_SYSCLK_LOW_MHZ);
    AppClock_ApplyHSITrim();
    (void)AppClock_ConfigAudioClockSelected();
    AudioAcq_RateMon_NotifyClockChanged(SystemCoreClock);
  }
#endif
}

void App_SystemClock_SetHigh(void)
{
#if (APP_SYSCLK_DYNAMIC_INFER_BOOST != 0)
  if (s_app_sysclk_mhz != APP_SYSCLK_HIGH_MHZ)
  {
    App_SystemClock_ConfigMHz(APP_SYSCLK_HIGH_MHZ);
    AppClock_ApplyHSITrim();
    (void)AppClock_ConfigAudioClockSelected();
    AudioAcq_RateMon_NotifyClockChanged(SystemCoreClock);
  }
#endif
}

uint32_t App_SystemClock_GetCurrentMHz(void)
{
  return s_app_sysclk_mhz;
}

/* USER CODE END 0 */

volatile uint32_t g_app_boot_tick_ms = 0U;

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  g_app_boot_tick_ms = HAL_GetTick();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the System Power */
  SystemPower_Config();

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ICACHE_Init();
  DebugUart_Init();
  if (AppClock_ConfigAudioClockSelected() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN 2 */
  App_TimingPulse_Event(APP_TIMING_EVENT_MAIN_READY);
  APP_MAIN_LOG_PRINT("\r\n[BOOT] Tx_LowPower timing/accuracy diag start\r\n");
  APP_MAIN_LOG_PRINT("[TPIN] PA8 pulses: 1=main-ready 2=neai-init-done 3=audio-start 4=first-dma-block 5=first-neai-window-ready 6=first-neai-start 7=first-neai-done 8=first-usc-done\r\n");
  APP_MAIN_LOG_PRINT("[TBOOT] main-ready t=%lu ms\r\n",
                     (unsigned long)(HAL_GetTick() - g_app_boot_tick_ms));
  APP_MAIN_LOG_PRINT("[CLK] core=%lu sys=%luMHz\r\n",
                     (unsigned long)SystemCoreClock,
                     (unsigned long)App_SystemClock_GetCurrentMHz());
  APP_MAIN_LOG_PRINT("[AUDIO] adcclk=%s %luHz\r\n",
                     AppClock_GetAudioAdcClkSrcName(),
                     (unsigned long)AppClock_GetAudioAdcClkHz());
  APP_MAIN_LOG_PRINT("[NEAI-CFG] fs=%u frame=%u hop=%u chunk=%u\r\n",
                     (unsigned)AUDIO_SAMPLE_RATE_HZ,
                     (unsigned)AUDIO_NEAI_FRAME_SAMPLES,
                     (unsigned)AUDIO_NEAI_HOP_SAMPLES,
                     (unsigned)AUDIO_DMA_CHUNK_SAMPLES);
  APP_MAIN_LOG_PRINT("[DIAG] timing=on accuracy=on mode=%u\r\n",
                     (unsigned)APP_PWR_PROFILE_MODE);
  /* USER CODE END 2 */

  MX_ThreadX_Init();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  App_SystemClock_ConfigMHz(APP_SYSCLK_BOOT_MHZ);
}

/**
  * @brief Power Configuration
  * @retval None
  */
static void SystemPower_Config(void)
{
  HAL_PWREx_EnableVddIO2();

  /*
   * Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral
   */
  HAL_PWREx_DisableUCPDDeadBattery();

  /*
   * Switch to SMPS regulator instead of LDO
   */
//  if (HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) != HAL_OK)
//  {
//    Error_Handler();
//  }
/* USER CODE BEGIN PWR */
/* USER CODE END PWR */
}

/**
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
static void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */

  /** Enable instruction cache in 1-way (direct mapped cache)
  */
  if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);

  /* Keep PA5 low by default to avoid external rail/BLE leakage during profiling */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : BUTTON_USER_Pin */
  GPIO_InitStruct.Pin = BUTTON_USER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BUTTON_USER_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LED3_Pin */
  GPIO_InitStruct.Pin = LED3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED3_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LED1_Pin */
  GPIO_InitStruct.Pin = LED1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED1_GPIO_Port, &GPIO_InitStruct);

  /* Configure PA5 as low-speed push-pull output, default LOW */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Configure PA8 as timing pulse output, default LOW */
  HAL_GPIO_WritePin(APP_TIMING_PULSE_GPIO_Port, APP_TIMING_PULSE_Pin, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = APP_TIMING_PULSE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(APP_TIMING_PULSE_GPIO_Port, &GPIO_InitStruct);

  /* Configure PC6 as stage-mark output, default LOW */
  HAL_GPIO_WritePin(APP_STAGE_MARK_PULSE_GPIO_Port, APP_STAGE_MARK_PULSE_Pin, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = APP_STAGE_MARK_PULSE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(APP_STAGE_MARK_PULSE_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI13_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI13_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
  while (1)
  {
    HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin);
    HAL_Delay(1000);
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* Infinite loop */
  while (1)
  {
  }
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
