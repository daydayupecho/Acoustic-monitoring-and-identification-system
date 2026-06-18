#ifndef AUDIO_ACQ_H
#define AUDIO_ACQ_H

#include "audio_config.h"
#include "power_test_cfg.h"
#include "stm32u5xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct
{
  const int16_t *data;
  uint16_t sample_count;
  uint32_t block_index;
} AudioAcq_Block_t;

typedef struct
{
  uint32_t lptim_cc1_count;        /* LPTIM1 compare/CC1 match 次数 */
  uint32_t dma_half_count;         /* DMA half callback 次数 */
  uint32_t dma_full_count;         /* DMA full callback 次数 */
  uint32_t dma_block_count;        /* DMA 块数（half+full） */
  uint32_t dma_sample_count;       /* DMA 样本数 */
  uint32_t queue_overwrite_count;  /* 队列满导致覆盖的次数 */
  uint32_t queue_depth;            /* 当前待处理 block 数 */
} AudioAcq_Diag_t;

extern ADC_HandleTypeDef hadc1;
extern LPTIM_HandleTypeDef hlptim1;
extern DMA_HandleTypeDef handle_GPDMA1_Channel4;

void AudioAcq_Init(void);
HAL_StatusTypeDef AudioAcq_Start(void);
void AudioAcq_Stop(void);
uint8_t AudioAcq_IsStarted(void);

int16_t *AudioAcq_GetDmaBuffer(void);
uint32_t AudioAcq_GetDmaBufferSampleCount(void);
void AudioAcq_OnHalfTransfer(void);
void AudioAcq_OnTransferComplete(void);

HAL_StatusTypeDef AudioAcq_FeedBlock(const int16_t *samples, uint16_t sample_count);
uint8_t AudioAcq_GetNextBlock(AudioAcq_Block_t *out_block);

uint32_t AudioAcq_GetMeasuredSampleRate_x100(void);
void AudioAcq_ResetMeasuredSampleRate(void);
void AudioAcq_RateMon_NotifyClockChanged(uint32_t new_clk_hz);

void AudioAcq_GetDiag(AudioAcq_Diag_t *out_diag);
void AudioAcq_LptimPeriodIrqHandler(void);
uint32_t AudioAcq_TakeCommittedBlockCount(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ACQ_H */
