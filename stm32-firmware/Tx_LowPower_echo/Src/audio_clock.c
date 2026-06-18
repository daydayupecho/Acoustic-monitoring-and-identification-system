#include "main.h"
#include "power_test_cfg.h"
#include "stm32u5xx_hal.h"

static const char *s_audio_adc_src_name = "MSIK";
static uint32_t s_audio_adc_clk_hz = APP_AUDIO_ADC_FREQ_HZ;

static void AppClock_ForceMSIKRange(uint32_t msik_range)
{
  SET_BIT(RCC->CR, RCC_CR_MSIKON);
  while ((RCC->CR & RCC_CR_MSIKRDY) == 0U) {}
  SET_BIT(RCC->ICSCR1, RCC_ICSCR1_MSIRGSEL);
  MODIFY_REG(RCC->ICSCR1, RCC_ICSCR1_MSIKRANGE_Msk, msik_range);
}

void AppClock_ApplyHSITrim(void)
{
  SET_BIT(RCC->CR, RCC_CR_HSION);
  while ((RCC->CR & RCC_CR_HSIRDY) == 0U) {}
  MODIFY_REG(RCC->ICSCR3, RCC_ICSCR3_HSITRIM_Msk, ((uint32_t)APP_HSITRIM_VALUE << RCC_ICSCR3_HSITRIM_Pos));
}

const char *AppClock_GetAudioAdcClkSrcName(void) { return s_audio_adc_src_name; }
uint32_t AppClock_GetAudioAdcClkHz(void) { return s_audio_adc_clk_hz; }

HAL_StatusTypeDef AppClock_ConfigAudioClockSelected(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  AppClock_ForceMSIKRange(APP_AUDIO_MSIK_RANGE);
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADCDAC;
  PeriphClkInit.AdcDacClockSelection = RCC_ADCDACCLKSOURCE_MSIK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    return HAL_ERROR;
  }
  s_audio_adc_src_name = "MSIK";
  s_audio_adc_clk_hz = APP_AUDIO_ADC_FREQ_HZ;
  return HAL_OK;
}
