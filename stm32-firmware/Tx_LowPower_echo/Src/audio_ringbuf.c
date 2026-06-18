#include "audio_config.h"
#include "audio_ringbuf.h"

#include <string.h>

static int16_t s_ring[AUDIO_RINGBUF_SAMPLES];
static uint32_t s_write_index = 0U;
static uint32_t s_count = 0U;
static uint32_t s_total_written = 0U;

void AudioRingBuf_Init(void)
{
  AudioRingBuf_Reset();
}

void AudioRingBuf_Reset(void)
{
  (void)memset(s_ring, 0, sizeof(s_ring));
  s_write_index = 0U;
  s_count = 0U;
  s_total_written = 0U;
}

HAL_StatusTypeDef AudioRingBuf_PushSamples(const int16_t *samples, uint32_t sample_count)
{
  uint32_t i;

  if ((samples == NULL) || (sample_count == 0U))
  {
    return HAL_ERROR;
  }

  for (i = 0U; i < sample_count; ++i)
  {
    s_ring[s_write_index] = samples[i];
    s_write_index = (s_write_index + 1U) % AUDIO_RINGBUF_SAMPLES;
    if (s_count < AUDIO_RINGBUF_SAMPLES)
    {
      s_count++;
    }
    s_total_written++;
  }

  return HAL_OK;
}

uint32_t AudioRingBuf_GetCount(void)
{
  return s_count;
}

uint32_t AudioRingBuf_GetCapacity(void)
{
  return AUDIO_RINGBUF_SAMPLES;
}

uint32_t AudioRingBuf_GetTotalWritten(void)
{
  return s_total_written;
}

uint32_t AudioRingBuf_GetOldestAbsoluteIndex(void)
{
  if (s_total_written <= s_count)
  {
    return 0U;
  }
  return (s_total_written - s_count);
}

HAL_StatusTypeDef AudioRingBuf_GetViewByAbsoluteIndex(uint32_t start_index,
                                                      uint32_t sample_count,
                                                      AudioRingBuf_View_t *view)
{
  uint32_t oldest;
  uint32_t newest_exclusive;
  uint32_t ring_index;
  uint32_t first_len;

  if ((view == NULL) || (sample_count == 0U))
  {
    return HAL_ERROR;
  }

  oldest = AudioRingBuf_GetOldestAbsoluteIndex();
  newest_exclusive = s_total_written;

  if ((start_index < oldest) || ((start_index + sample_count) > newest_exclusive))
  {
    return HAL_ERROR;
  }

  ring_index = start_index % AUDIO_RINGBUF_SAMPLES;
  first_len = AUDIO_RINGBUF_SAMPLES - ring_index;
  if (first_len > sample_count)
  {
    first_len = sample_count;
  }

  view->seg1_ptr = &s_ring[ring_index];
  view->seg1_count = first_len;
  view->seg2_ptr = (first_len < sample_count) ? &s_ring[0] : NULL;
  view->seg2_count = sample_count - first_len;
  view->start_index = start_index;
  view->sample_count = sample_count;

  return HAL_OK;
}

HAL_StatusTypeDef AudioRingBuf_ReadByAbsoluteIndex(uint32_t start_index,
                                                   int16_t *dst,
                                                   uint32_t sample_count)
{
  AudioRingBuf_View_t view;

  if ((dst == NULL) || (sample_count == 0U))
  {
    return HAL_ERROR;
  }

  if (AudioRingBuf_GetViewByAbsoluteIndex(start_index, sample_count, &view) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (view.seg1_count != 0U)
  {
    (void)memcpy(dst, view.seg1_ptr, view.seg1_count * sizeof(int16_t));
  }
  if (view.seg2_count != 0U)
  {
    (void)memcpy(&dst[view.seg1_count], view.seg2_ptr, view.seg2_count * sizeof(int16_t));
  }

  return HAL_OK;
}

HAL_StatusTypeDef AudioRingBuf_ReadLatest(int16_t *dst, uint32_t sample_count)
{
  if (sample_count > s_count)
  {
    return HAL_ERROR;
  }

  return AudioRingBuf_ReadByAbsoluteIndex(s_total_written - sample_count, dst, sample_count);
}
