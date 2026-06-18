
#ifndef USC_PREPROC_H
#define USC_PREPROC_H
#include "stm32u5xx_hal.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
HAL_StatusTypeDef UscPreproc_Init(void);
uint32_t UscPreproc_GetExpectedFrameSamples(void);
uint32_t UscPreproc_GetExpectedFeatureCount(void);
uint32_t UscPreproc_GetNumColumns(void);
uint32_t UscPreproc_GetNumCoeffsPerColumn(void);
HAL_StatusTypeDef UscPreproc_ProcessColumnsView(const int16_t *seg1_ptr,
                                                uint32_t seg1_count,
                                                const int16_t *seg2_ptr,
                                                uint32_t seg2_count,
                                                uint32_t start_col,
                                                uint32_t col_count,
                                                float *out_features,
                                                uint32_t out_count);
HAL_StatusTypeDef UscPreproc_ProcessFrameView(const int16_t *seg1_ptr,
                                              uint32_t seg1_count,
                                              const int16_t *seg2_ptr,
                                              uint32_t seg2_count,
                                              float *out_features,
                                              uint32_t out_count);
#ifdef __cplusplus
}
#endif
#endif
