
#include "usc_preproc.h"
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "feature_extraction.h"
#include "audio_config.h"
#include "arm_math.h"
#include "usc_preproc_tables.h"
#include "usc_preproc_incremental.h"

#define USC_PREPROC_SAMPLE_RATE       (AUDIO_SAMPLE_RATE_HZ)
#define USC_PREPROC_FFT_LEN           (AUDIO_USC_FFT_LEN)
#define USC_PREPROC_FRAME_LEN         (AUDIO_USC_FRAME_LEN)
#define USC_PREPROC_FRAME_ADVANCE     (AUDIO_USC_FRAME_LEN)
#define USC_PREPROC_NUM_MFCC          (AUDIO_USC_NUM_MFCC)
#define USC_PREPROC_NUM_COLUMNS       (AUDIO_USC_NUM_TIME_STEPS)
#define USC_PREPROC_NUM_MELS          (AUDIO_USC_NUM_MFCC)
#define USC_PREPROC_EXPECTED_SAMPLES  (AUDIO_USC_FRAME_SAMPLES)
#define USC_PREPROC_EXPECTED_FEATURES (USC_PREPROC_NUM_COLUMNS * USC_PREPROC_NUM_MFCC)

#if defined(__GNUC__)
#define APP_ALIGN16 __attribute__((aligned(16)))
#else
#define APP_ALIGN16
#endif

static APP_ALIGN16 arm_rfft_fast_instance_f32 s_rfft;
static APP_ALIGN16 MelFilterTypeDef s_mel_filter;
static APP_ALIGN16 DCT_InstanceTypeDef s_dct;
static APP_ALIGN16 SpectrogramTypeDef s_spectr;
static APP_ALIGN16 MelSpectrogramTypeDef s_mel_spectr;
static APP_ALIGN16 LogMelSpectrogramTypeDef s_logmel_spectr;
static APP_ALIGN16 MfccTypeDef s_mfcc;

static APP_ALIGN16 float32_t s_in_frame[USC_PREPROC_FRAME_LEN];
static APP_ALIGN16 int16_t s_in_frame_i16[USC_PREPROC_FRAME_LEN];
static APP_ALIGN16 float32_t s_scratch_spectr[USC_PREPROC_FFT_LEN];
static APP_ALIGN16 float32_t s_mfcc_scratch[USC_PREPROC_NUM_MFCC];
static APP_ALIGN16 float32_t s_out_col[USC_PREPROC_NUM_MFCC];
static uint8_t s_inited = 0U;

static int16_t UscPreproc_GetSampleAt(const int16_t *seg1_ptr,
                                      uint32_t seg1_count,
                                      const int16_t *seg2_ptr,
                                      uint32_t seg2_count,
                                      uint32_t idx)
{
  if ((seg1_ptr != NULL) && (idx < seg1_count))
  {
    return seg1_ptr[idx];
  }
  idx -= seg1_count;
  if ((seg2_ptr != NULL) && (idx < seg2_count))
  {
    return seg2_ptr[idx];
  }
  return 0;
}

HAL_StatusTypeDef UscPreproc_Init(void)
{
  if (s_inited != 0U)
  {
    return HAL_OK;
  }
  if (arm_rfft_fast_init_f32(&s_rfft, USC_PREPROC_FFT_LEN) != ARM_MATH_SUCCESS)
  {
    return HAL_ERROR;
  }

  s_mel_filter.pStartIndices = (uint32_t*)melFiltersStartIndices_512_32;
  s_mel_filter.pStopIndices  = (uint32_t*)melFiltersStopIndices_512_32;
  s_mel_filter.pCoefficients = (float32_t*)melFilterLut_512_32;
  s_mel_filter.CoefficientsLength = 485U;
  s_mel_filter.NumMels = USC_PREPROC_NUM_MELS;
  s_mel_filter.FFTLen = USC_PREPROC_FFT_LEN;
  s_mel_filter.SampRate = USC_PREPROC_SAMPLE_RATE;
  s_mel_filter.FMin = 125.0f;
  s_mel_filter.FMax = USC_PREPROC_SAMPLE_RATE / 2.0f;
  s_mel_filter.Formula = MEL_SLANEY;
  s_mel_filter.Normalize = 1U;
  s_mel_filter.Mel2F = 1U;

  s_dct.NumFilters = USC_PREPROC_NUM_MFCC;
  s_dct.NumInputs = USC_PREPROC_NUM_MFCC;
  s_dct.Type = DCT_TYPE_II_ORTHO;
  s_dct.RemoveDCTZero = 0;
  s_dct.pDCTCoefs = (float32_t*)dct2_32_32;

  s_spectr.pRfft = &s_rfft;
  s_spectr.Type = SPECTRUM_TYPE_POWER;
  s_spectr.pWindow = (float32_t*)hannWin_512;
  s_spectr.SampRate = USC_PREPROC_SAMPLE_RATE;
  s_spectr.FrameLen = USC_PREPROC_FRAME_LEN;
  s_spectr.FFTLen = USC_PREPROC_FFT_LEN;
  s_spectr.pScratch = s_scratch_spectr;

  s_mel_spectr.SpectrogramConf = &s_spectr;
  s_mel_spectr.MelFilter = &s_mel_filter;

  s_logmel_spectr.MelSpectrogramConf = &s_mel_spectr;
  s_logmel_spectr.LogFormula = LOGMELSPECTROGRAM_SCALE_DB;
  s_logmel_spectr.Ref = 1.0f;
  s_logmel_spectr.TopdB = HUGE_VALF;

  s_mfcc.LogMelConf = &s_logmel_spectr;
  s_mfcc.pDCT = &s_dct;
  s_mfcc.NumMfccCoefs = USC_PREPROC_NUM_MFCC;
  s_mfcc.pScratch = s_mfcc_scratch;

  s_inited = 1U;
  return HAL_OK;
}

uint32_t UscPreproc_GetExpectedFrameSamples(void)
{
  return USC_PREPROC_EXPECTED_SAMPLES;
}

uint32_t UscPreproc_GetExpectedFeatureCount(void)
{
  return USC_PREPROC_EXPECTED_FEATURES;
}

uint32_t UscPreproc_GetNumColumns(void)
{
  return USC_PREPROC_NUM_COLUMNS;
}

uint32_t UscPreproc_GetNumCoeffsPerColumn(void)
{
  return USC_PREPROC_NUM_MFCC;
}

HAL_StatusTypeDef UscPreproc_ProcessColumnsView(const int16_t *seg1_ptr,
                                                uint32_t seg1_count,
                                                const int16_t *seg2_ptr,
                                                uint32_t seg2_count,
                                                uint32_t start_col,
                                                uint32_t col_count,
                                                float *out_features,
                                                uint32_t out_count)
{
  uint32_t total = seg1_count + seg2_count;
  uint32_t col;

  if ((out_features == NULL) || (col_count == 0U) ||
      (start_col >= USC_PREPROC_NUM_COLUMNS) ||
      ((start_col + col_count) > USC_PREPROC_NUM_COLUMNS) ||
      (out_count != (col_count * USC_PREPROC_NUM_MFCC)))
  {
    return HAL_ERROR;
  }

  if (UscPreproc_Init() != HAL_OK)
  {
    return HAL_ERROR;
  }

  if ((seg1_ptr == NULL) || (total < USC_PREPROC_EXPECTED_SAMPLES))
  {
    return HAL_ERROR;
  }

  for (col = 0U; col < col_count; col++)
  {
    uint32_t n;
    uint32_t src_col = start_col + col;
    uint32_t frame_start = src_col * USC_PREPROC_FRAME_ADVANCE;

    for (n = 0U; n < USC_PREPROC_FRAME_LEN; n++)
    {
      s_in_frame_i16[n] = UscPreproc_GetSampleAt(seg1_ptr, seg1_count, seg2_ptr, seg2_count, frame_start + n);
    }

    buf_to_float(s_in_frame_i16, s_in_frame, USC_PREPROC_FRAME_LEN);
    MfccColumn(&s_mfcc, s_in_frame, s_out_col);
    memcpy(&out_features[col * USC_PREPROC_NUM_MFCC], s_out_col, sizeof(s_out_col));
  }

  return HAL_OK;
}

HAL_StatusTypeDef UscPreproc_ProcessFrameView(const int16_t *seg1_ptr,
                                              uint32_t seg1_count,
                                              const int16_t *seg2_ptr,
                                              uint32_t seg2_count,
                                              float *out_features,
                                              uint32_t out_count)
{
  return UscPreproc_ProcessColumnsView(seg1_ptr, seg1_count,
                                       seg2_ptr, seg2_count,
                                       0U, USC_PREPROC_NUM_COLUMNS,
                                       out_features, out_count);
}


static UscPreprocInc_Status_t UscPreprocInc_RunFull(UscPreprocIncremental_t *ctx,
                                                    const AudioWindow_View_t *view,
                                                    float *feat_out,
                                                    UscPreprocInc_Mode_t mode,
                                                    UscPreprocInc_Mode_t *mode_out,
                                                    uint32_t *new_cols_out)
{
  if ((ctx == NULL) || (view == NULL) || (feat_out == NULL))
  {
    return USC_PREINC_ERR_ARG;
  }

  if (UscPreproc_ProcessFrameView(view->seg1_ptr,
                                  view->seg1_count,
                                  view->seg2_ptr,
                                  view->seg2_count,
                                  feat_out,
                                  ctx->feature_count) != HAL_OK)
  {
    return USC_PREINC_ERR_PREPROC;
  }

  if (ctx->feature_count <= USC_PREPROC_INC_MAX_FEATURES)
  {
    memcpy(ctx->feature_cache, feat_out, ctx->feature_count * sizeof(float));
  }

  ctx->valid = 1U;
  ctx->last_start_index = view->start_index;

  if (mode_out != NULL)
  {
    *mode_out = mode;
  }
  if (new_cols_out != NULL)
  {
    *new_cols_out = ctx->cols_total;
  }
  return USC_PREINC_OK;
}

void UscPreprocIncremental_Reset(UscPreprocIncremental_t *ctx)
{
  if (ctx == NULL)
  {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
}

UscPreprocInc_Status_t UscPreprocIncremental_Init(UscPreprocIncremental_t *ctx,
                                                  uint32_t frame_samples,
                                                  uint32_t hop_samples,
                                                  uint32_t cols_total,
                                                  uint32_t coeffs_per_col,
                                                  uint32_t cols_per_hop)
{
  uint32_t feature_count;

  if ((ctx == NULL) || (frame_samples == 0U) || (hop_samples == 0U) ||
      (cols_total == 0U) || (coeffs_per_col == 0U) || (cols_per_hop == 0U))
  {
    return USC_PREINC_ERR_ARG;
  }

  feature_count = UscPreproc_GetExpectedFeatureCount();
  if ((feature_count == 0U) || (feature_count > USC_PREPROC_INC_MAX_FEATURES))
  {
    return USC_PREINC_ERR_CONFIG;
  }

  memset(ctx, 0, sizeof(*ctx));
  ctx->frame_samples = frame_samples;
  ctx->hop_samples = hop_samples;
  ctx->feature_count = feature_count;
  ctx->cols_total = cols_total;
  ctx->coeffs_per_col = coeffs_per_col;
  ctx->cols_per_hop = cols_per_hop;
  return USC_PREINC_OK;
}

UscPreprocInc_Status_t UscPreprocIncremental_ProcessView(UscPreprocIncremental_t *ctx,
                                                         const AudioWindow_View_t *view,
                                                         float *feat_out,
                                                         UscPreprocInc_Mode_t *mode_out,
                                                         uint32_t *new_cols_out)
{
  uint32_t expected_next;

  if ((ctx == NULL) || (view == NULL) || (feat_out == NULL))
  {
    return USC_PREINC_ERR_ARG;
  }

  if (ctx->valid == 0U)
  {
    return UscPreprocInc_RunFull(ctx, view, feat_out,
                                 USC_PREINC_MODE_FULL_BOOT,
                                 mode_out, new_cols_out);
  }

  expected_next = ctx->last_start_index + ctx->hop_samples;
  if (view->start_index != expected_next)
  {
    return UscPreprocInc_RunFull(ctx, view, feat_out,
                                 USC_PREINC_MODE_FULL_RESET,
                                 mode_out, new_cols_out);
  }

  if ((ctx->cols_per_hop >= ctx->cols_total) ||
      (ctx->coeffs_per_col == 0U) ||
      (ctx->feature_count != (ctx->cols_total * ctx->coeffs_per_col)))
  {
    return UscPreprocInc_RunFull(ctx, view, feat_out,
                                 USC_PREINC_MODE_CONTIG_FALLBACK,
                                 mode_out, new_cols_out);
  }

  {
    uint32_t shift_cols = ctx->cols_per_hop;
    uint32_t keep_cols = ctx->cols_total - shift_cols;
    uint32_t shift_feats = shift_cols * ctx->coeffs_per_col;
    uint32_t keep_feats = keep_cols * ctx->coeffs_per_col;

    memmove(&ctx->feature_cache[0],
            &ctx->feature_cache[shift_feats],
            keep_feats * sizeof(float));

    if (UscPreproc_ProcessColumnsView(view->seg1_ptr,
                                      view->seg1_count,
                                      view->seg2_ptr,
                                      view->seg2_count,
                                      keep_cols,
                                      shift_cols,
                                      &ctx->feature_cache[keep_feats],
                                      shift_feats) != HAL_OK)
    {
      return UscPreprocInc_RunFull(ctx, view, feat_out,
                                   USC_PREINC_MODE_CONTIG_FALLBACK,
                                   mode_out, new_cols_out);
    }

    memcpy(feat_out, ctx->feature_cache, ctx->feature_count * sizeof(float));
    ctx->valid = 1U;
    ctx->last_start_index = view->start_index;

    if (mode_out != NULL)
    {
      *mode_out = USC_PREINC_MODE_CONTIG_INC;
    }
    if (new_cols_out != NULL)
    {
      *new_cols_out = shift_cols;
    }
    return USC_PREINC_OK;
  }
}
