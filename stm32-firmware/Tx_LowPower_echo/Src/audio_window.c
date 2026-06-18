#include "audio_window.h"

#include "audio_ringbuf.h"
#include "power_test_cfg.h"
#include "debug_uart.h"
#include <string.h>

static AudioWindow_Config_t s_cfg;
static uint32_t s_total_written_snapshot = 0U;
static uint32_t s_next_window_start = 0U;
static uint8_t s_ready = 0U;
static AudioWindow_Diag_t s_diag;

static uint32_t AudioWindow_ComputeReadyBacklog(uint32_t total_samples_written)
{
  if ((s_cfg.frame_samples == 0U) || (s_cfg.hop_samples == 0U))
  {
    return 0U;
  }

  if ((s_next_window_start + s_cfg.frame_samples) > total_samples_written)
  {
    return 0U;
  }

  return 1U + ((total_samples_written - (s_next_window_start + s_cfg.frame_samples)) / s_cfg.hop_samples);
}

void AudioWindow_Init(const AudioWindow_Config_t *config)
{
  if (config != NULL)
  {
    s_cfg = *config;
  }
  else
  {
    (void)memset(&s_cfg, 0, sizeof(s_cfg));
  }

  AudioWindow_Reset();
}

void AudioWindow_Reset(void)
{
  s_total_written_snapshot = 0U;
  s_next_window_start = 0U;
  s_ready = 0U;
  (void)memset(&s_diag, 0, sizeof(s_diag));
}


void AudioWindow_SeekToLatestReady(uint32_t total_samples_written)
{
  uint32_t oldest;
  uint32_t latest_start = 0U;

  s_total_written_snapshot = total_samples_written;
  oldest = AudioRingBuf_GetOldestAbsoluteIndex();

  if (s_cfg.frame_samples != 0U)
  {
    if (total_samples_written > s_cfg.frame_samples)
    {
      latest_start = total_samples_written - s_cfg.frame_samples;
    }

    if (latest_start < oldest)
    {
      latest_start = oldest;
    }

    s_next_window_start = latest_start;
    s_ready = (((s_next_window_start + s_cfg.frame_samples) <= s_total_written_snapshot) ? 1U : 0U);
  }
  else
  {
    s_next_window_start = oldest;
    s_ready = 0U;
  }

  s_diag.last_total_written = total_samples_written;
  s_diag.last_oldest = oldest;
  s_diag.last_next_before = s_next_window_start;
  s_diag.last_next_after = s_next_window_start;
  s_diag.ready_backlog = AudioWindow_ComputeReadyBacklog(s_total_written_snapshot);
  if (s_diag.ready_backlog > s_diag.max_ready_backlog)
  {
    s_diag.max_ready_backlog = s_diag.ready_backlog;
  }
}
void AudioWindow_UpdateOnSamples(uint32_t total_samples_written)
{
  uint32_t oldest;
  uint32_t next_before;

  s_total_written_snapshot = total_samples_written;
  oldest = AudioRingBuf_GetOldestAbsoluteIndex();
  next_before = s_next_window_start;

  s_diag.update_calls++;
  s_diag.last_total_written = total_samples_written;
  s_diag.last_oldest = oldest;
  s_diag.last_next_before = next_before;

  if (s_next_window_start < oldest)
  {
    uint32_t delta = oldest - s_next_window_start;
    uint32_t skip_hop = 0U;
    if (s_cfg.hop_samples != 0U)
    {
      skip_hop = (delta + s_cfg.hop_samples - 1U) / s_cfg.hop_samples;
    }
    s_diag.clamp_events++;
    s_diag.clamped_samples += delta;
    s_diag.clamped_hops += skip_hop;
#if (APP_AUDIO_WINDOW_JUMP_LOG_EN != 0)
    Debug_Print("[WIN-JMP] clamp old=%lu nextB=%lu oldest=%lu delta=%lu hop=%lu skip=%lu\r\n",
                (unsigned long)s_diag.last_oldest,
                (unsigned long)next_before,
                (unsigned long)oldest,
                (unsigned long)delta,
                (unsigned long)s_cfg.hop_samples,
                (unsigned long)skip_hop);
#endif
    s_next_window_start = oldest;
  }

  if ((s_cfg.frame_samples != 0U) && ((s_next_window_start + s_cfg.frame_samples) <= s_total_written_snapshot))
  {
    s_ready = 1U;
  }

  s_diag.last_next_after = s_next_window_start;
  s_diag.ready_backlog = AudioWindow_ComputeReadyBacklog(s_total_written_snapshot);
  if (s_diag.ready_backlog > s_diag.max_ready_backlog)
  {
    s_diag.max_ready_backlog = s_diag.ready_backlog;
  }
}

uint8_t AudioWindow_HasReadyWindow(void)
{
  if ((s_ready == 0U) && (s_cfg.frame_samples != 0U) &&
      ((s_next_window_start + s_cfg.frame_samples) <= s_total_written_snapshot))
  {
    s_ready = 1U;
  }
  return s_ready;
}

HAL_StatusTypeDef AudioWindow_PopNextWindowView(AudioWindow_View_t *view)
{
  AudioRingBuf_View_t rb_view;
  HAL_StatusTypeDef status;
  uint32_t pop_start;
  uint32_t expected_next = 0U;
  uint8_t pop_jump = 0U;

  if ((view == NULL) || (s_cfg.frame_samples == 0U))
  {
    return HAL_ERROR;
  }

  if (AudioWindow_HasReadyWindow() == 0U)
  {
    return HAL_BUSY;
  }

  pop_start = s_next_window_start;
  if ((s_diag.pop_calls > 0U) && (s_cfg.hop_samples != 0U))
  {
    expected_next = s_diag.last_pop_start + s_cfg.hop_samples;
    if (pop_start != expected_next)
    {
      pop_jump = 1U;
    }
  }

  status = AudioRingBuf_GetViewByAbsoluteIndex(pop_start, s_cfg.frame_samples, &rb_view);
  if (status != HAL_OK)
  {
    return status;
  }

  if (pop_jump != 0U)
  {
#if (APP_AUDIO_WINDOW_JUMP_LOG_EN != 0)
    Debug_Print("[WIN-JMP] pop last=%lu expect=%lu actual=%lu hop=%lu\r\n",
                (unsigned long)s_diag.last_pop_start,
                (unsigned long)expected_next,
                (unsigned long)pop_start,
                (unsigned long)s_cfg.hop_samples);
#endif
  }

  view->start_index = pop_start;
  view->sample_count = s_cfg.frame_samples;
  view->seg1_ptr = rb_view.seg1_ptr;
  view->seg1_count = rb_view.seg1_count;
  view->seg2_ptr = rb_view.seg2_ptr;
  view->seg2_count = rb_view.seg2_count;

  s_diag.pop_calls++;
  s_diag.last_pop_start = view->start_index;

  s_next_window_start = pop_start + s_cfg.hop_samples;
  s_ready = 0U;
  AudioWindow_UpdateOnSamples(AudioRingBuf_GetTotalWritten());
  return HAL_OK;
}

HAL_StatusTypeDef AudioWindow_PopNextWindow(int16_t *dst,
                                            uint32_t dst_capacity,
                                            AudioWindow_Info_t *info)
{
  AudioWindow_View_t view;

  if ((dst == NULL) || (dst_capacity < s_cfg.frame_samples) || (s_cfg.frame_samples == 0U))
  {
    return HAL_ERROR;
  }

  if (AudioWindow_PopNextWindowView(&view) != HAL_OK)
  {
    return HAL_BUSY;
  }

  if (view.seg1_count != 0U)
  {
    (void)memcpy(dst, view.seg1_ptr, view.seg1_count * sizeof(int16_t));
  }
  if (view.seg2_count != 0U)
  {
    (void)memcpy(&dst[view.seg1_count], view.seg2_ptr, view.seg2_count * sizeof(int16_t));
  }

  if (info != NULL)
  {
    info->start_index = view.start_index;
    info->sample_count = view.sample_count;
  }

  return HAL_OK;
}

void AudioWindow_SetHopSamples(uint32_t hop_samples)
{
  if (hop_samples != 0U)
  {
    s_cfg.hop_samples = hop_samples;
  }
}

void AudioWindow_GetConfig(AudioWindow_Config_t *config)
{
  if (config != NULL)
  {
    *config = s_cfg;
  }
}

void AudioWindow_GetDiag(AudioWindow_Diag_t *diag)
{
  if (diag != NULL)
  {
    *diag = s_diag;
  }
}
