/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void MX_GPIO_Init(void);
void App_SystemClock_SetLow(void);
void App_SystemClock_SetHigh(void);
uint32_t App_SystemClock_GetCurrentMHz(void);
void App_TimingPulse_Event(uint32_t event_id);
void App_StageMark_Set(GPIO_PinState state);
extern volatile uint32_t g_app_boot_tick_ms;

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BUTTON_USER_Pin GPIO_PIN_13
#define BUTTON_USER_GPIO_Port GPIOC
#define BUTTON_USER_EXTI_IRQn EXTI13_IRQn
#define LED3_Pin GPIO_PIN_2
#define LED3_GPIO_Port GPIOG
#define LED1_Pin GPIO_PIN_7
#define LED1_GPIO_Port GPIOC

/* USER CODE BEGIN Private defines */
#define APP_TIMING_PULSE_Pin GPIO_PIN_8
#define APP_TIMING_PULSE_GPIO_Port GPIOA
#define APP_STAGE_MARK_PULSE_Pin GPIO_PIN_6
#define APP_STAGE_MARK_PULSE_GPIO_Port GPIOC

#define APP_TIMING_EVENT_MAIN_READY             1U
#define APP_TIMING_EVENT_NEAI_INIT_DONE         2U
#define APP_TIMING_EVENT_AUDIO_START            3U
#define APP_TIMING_EVENT_FIRST_DMA_BLOCK        4U
#define APP_TIMING_EVENT_FIRST_NEAI_WIN_READY   5U
#define APP_TIMING_EVENT_FIRST_NEAI_START       6U
#define APP_TIMING_EVENT_FIRST_NEAI_DONE        7U
#define APP_TIMING_EVENT_FIRST_USC_DONE         8U


/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
