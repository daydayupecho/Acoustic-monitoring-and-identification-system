#include "main.h"
#include "debug_uart.h"
#include "power_test_cfg.h"
#include "tx_api.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart1;

static TX_MUTEX s_debug_uart_mutex;
static volatile uint8_t s_debug_uart_mutex_ready = 0U;

static uint8_t DebugUart_ShouldUseMutex(void)
{
  if (s_debug_uart_mutex_ready == 0U)
  {
    return 0U;
  }

  if (__get_IPSR() != 0U)
  {
    return 0U;
  }

  return (tx_thread_identify() != TX_NULL) ? 1U : 0U;
}

static void DebugUart_Lock(void)
{
  if (DebugUart_ShouldUseMutex() != 0U)
  {
    (void)tx_mutex_get(&s_debug_uart_mutex, TX_WAIT_FOREVER);
  }
}

static void DebugUart_Unlock(void)
{
  if (DebugUart_ShouldUseMutex() != 0U)
  {
    (void)tx_mutex_put(&s_debug_uart_mutex);
  }
}

static void DebugUart_EnableHSI(void)
{
  __HAL_RCC_HSI_ENABLE();
  while (__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) == RESET)
  {
  }
}

void MX_USART1_UART_Init(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  DebugUart_EnableHSI();

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_HSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

  huart1.Instance = USART1;
#if (APP_RUN_MODE == APP_RUN_MODE_DATA_CAPTURE)
  huart1.Init.BaudRate = 921600;
#else
  huart1.Init.BaudRate = 115200;
#endif
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
#if (APP_RUN_MODE == APP_RUN_MODE_DATA_CAPTURE)
  huart1.Init.Mode = UART_MODE_TX_RX;
#else
  huart1.Init.Mode = UART_MODE_TX;
#endif
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

void DebugUart_ThreadXSyncInit(void)
{
  if (s_debug_uart_mutex_ready == 0U)
  {
    if (tx_mutex_create(&s_debug_uart_mutex, "debug uart", TX_INHERIT) == TX_SUCCESS)
    {
      s_debug_uart_mutex_ready = 1U;
    }
  }
}

void DebugUart_Init(void)
{
  MX_USART1_UART_Init();
}

void Debug_PutString(const char *s)
{
  if (s == NULL)
  {
    return;
  }

  DebugUart_Lock();
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 1000);
  DebugUart_Unlock();
}

void Debug_Print(const char *fmt, ...)
{
  char buf[320];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n < 0)
  {
    return;
  }
  size_t len = (size_t)((n < (int)sizeof(buf)) ? n : ((int)sizeof(buf) - 1));
  DebugUart_Lock();
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, 1000);
  DebugUart_Unlock();
}
