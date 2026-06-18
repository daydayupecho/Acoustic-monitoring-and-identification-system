#ifndef USC_AI_RUNNER_H
#define USC_AI_RUNNER_H

#include "stm32u5xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

HAL_StatusTypeDef UscAiRunner_Init(void);
void UscAiRunner_DeInit(void);
HAL_StatusTypeDef UscAiRunner_Run(const float *in_features,
                                  uint32_t in_count,
                                  float *out_scores,
                                  uint32_t out_count);

#ifdef __cplusplus
}
#endif

#endif /* USC_AI_RUNNER_H */
