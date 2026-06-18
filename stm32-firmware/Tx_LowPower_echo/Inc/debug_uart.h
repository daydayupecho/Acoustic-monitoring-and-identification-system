#ifndef DEBUG_UART_H
#define DEBUG_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32u5xx_hal.h"
#include <stdarg.h>

extern UART_HandleTypeDef huart1;

void MX_USART1_UART_Init(void);
void DebugUart_Init(void);
void DebugUart_ThreadXSyncInit(void);
void Debug_Print(const char *fmt, ...);
void Debug_PutString(const char *s);

#ifdef __cplusplus
}
#endif

#endif
