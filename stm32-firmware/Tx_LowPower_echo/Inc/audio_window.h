#ifndef AUDIO_WINDOW_H
#define AUDIO_WINDOW_H

#include "stm32u5xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint32_t frame_samples;
  uint32_t hop_samples;
} AudioWindow_Config_t;

typedef struct
{
  uint32_t start_index;
  uint32_t sample_count;
} AudioWindow_Info_t;

typedef struct
{
  uint32_t start_index;
  uint32_t sample_count;
  const int16_t *seg1_ptr;
  uint32_t seg1_count;
  const int16_t *seg2_ptr;
  uint32_t seg2_count;
} AudioWindow_View_t;

typedef struct
{
  uint32_t update_calls;
  uint32_t pop_calls;
  uint32_t clamp_events;
  uint32_t clamped_samples;
  uint32_t clamped_hops;
  uint32_t ready_backlog;
  uint32_t max_ready_backlog;
  uint32_t last_total_written;
  uint32_t last_oldest;
  uint32_t last_next_before;
  uint32_t last_next_after;
  uint32_t last_pop_start;
} AudioWindow_Diag_t;

void AudioWindow_Init(const AudioWindow_Config_t *config);
void AudioWindow_Reset(void);
void AudioWindow_UpdateOnSamples(uint32_t total_samples_written);
void AudioWindow_SeekToLatestReady(uint32_t total_samples_written);
uint8_t AudioWindow_HasReadyWindow(void);
HAL_StatusTypeDef AudioWindow_PopNextWindow(int16_t *dst,
                                            uint32_t dst_capacity,
                                            AudioWindow_Info_t *info);
HAL_StatusTypeDef AudioWindow_PopNextWindowView(AudioWindow_View_t *view);
void AudioWindow_SetHopSamples(uint32_t hop_samples);
void AudioWindow_GetConfig(AudioWindow_Config_t *config);
void AudioWindow_GetDiag(AudioWindow_Diag_t *diag);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_WINDOW_H */
