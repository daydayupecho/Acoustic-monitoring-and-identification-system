#ifndef AUDIO_RINGBUF_H
#define AUDIO_RINGBUF_H

#include "stm32u5xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  const int16_t *seg1_ptr;
  uint32_t seg1_count;
  const int16_t *seg2_ptr;
  uint32_t seg2_count;
  uint32_t start_index;
  uint32_t sample_count;
} AudioRingBuf_View_t;

void AudioRingBuf_Init(void);
void AudioRingBuf_Reset(void);
HAL_StatusTypeDef AudioRingBuf_PushSamples(const int16_t *samples, uint32_t sample_count);
uint32_t AudioRingBuf_GetCount(void);
uint32_t AudioRingBuf_GetCapacity(void);
uint32_t AudioRingBuf_GetTotalWritten(void);
uint32_t AudioRingBuf_GetOldestAbsoluteIndex(void);
HAL_StatusTypeDef AudioRingBuf_GetViewByAbsoluteIndex(uint32_t start_index,
                                                      uint32_t sample_count,
                                                      AudioRingBuf_View_t *view);
HAL_StatusTypeDef AudioRingBuf_ReadByAbsoluteIndex(uint32_t start_index,
                                                   int16_t *dst,
                                                   uint32_t sample_count);
HAL_StatusTypeDef AudioRingBuf_ReadLatest(int16_t *dst, uint32_t sample_count);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_RINGBUF_H */
