/**
 ******************************************************************************
 * @file    feature_extraction.c
 * @author  MCD Application Team
 * @brief   Spectral feature extraction functions
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Software License Agreement
 * SLA0055, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *        www.st.com/resource/en/license_agreement/dm00251784.pdf
 *
 ******************************************************************************
 */
#include "feature_extraction.h"
#include "debug_uart.h"
#include "power_test_cfg.h"
#include <stdint.h>
#include <string.h>

#if defined(__GNUC__)
#pragma GCC push_options
#pragma GCC optimize ("O0")
#endif

extern uint32_t AppDiag_DwtNow(void);
extern uint32_t AppDiag_DwtDeltaUs(uint32_t start, uint32_t end);


static uint32_t s_mfcc_deep_diag_count = 0U;
static uint8_t s_mfcc_deep_diag_active = 0U;
static uint32_t s_mfcc_deep_diag_base_cyc = 0U;


#if defined(__GNUC__)
#define FE_ALIGN16 __attribute__((aligned(16)))
#else
#define FE_ALIGN16
#endif

#define FE_CANARY_WORDS           (4U)
#define FE_CANARY_MAX_FFT         (512U)
#define FE_CANARY_MAX_SPEC_OUT    ((FE_CANARY_MAX_FFT / 2U) + 1U)
#define FE_CANARY_MAX_MFCC_TMP    (64U)

typedef struct
{
  uint32_t head[FE_CANARY_WORDS];
  FE_ALIGN16 float32_t data[FE_CANARY_MAX_FFT];
  uint32_t tail[FE_CANARY_WORDS];
} FeCanaryBufFft_t;

typedef struct
{
  uint32_t head[FE_CANARY_WORDS];
  FE_ALIGN16 float32_t data[FE_CANARY_MAX_SPEC_OUT];
  uint32_t tail[FE_CANARY_WORDS];
} FeCanaryBufSpec_t;

typedef struct
{
  uint32_t head[FE_CANARY_WORDS];
  FE_ALIGN16 float32_t data[FE_CANARY_MAX_MFCC_TMP];
  uint32_t tail[FE_CANARY_WORDS];
} FeCanaryBufMfcc_t;

static FeCanaryBufFft_t s_fe_canary_in;
static FeCanaryBufFft_t s_fe_canary_fft;
static FeCanaryBufSpec_t s_fe_canary_spec;
static FeCanaryBufMfcc_t s_fe_canary_mfcc_tmp;
static FeCanaryBufMfcc_t s_fe_canary_mfcc_out;
static uint32_t s_fe_canary_prints = 0U;
static uint8_t s_fe_canary_align_printed = 0U;

static uint32_t FeDiag_CanaryWord(uint32_t seed, uint32_t idx)
{
  return seed ^ (0x9E3779B9UL + (idx * 0x01010101UL));
}

static void FeDiag_InitCanaryWords(uint32_t *buf, uint32_t words, uint32_t seed)
{
  for (uint32_t i = 0U; i < words; i++)
  {
    buf[i] = FeDiag_CanaryWord(seed, i);
  }
}

static void FeDiag_PrepareCanaryFft(FeCanaryBufFft_t *buf, uint32_t seed)
{
  FeDiag_InitCanaryWords(buf->head, FE_CANARY_WORDS, seed);
  FeDiag_InitCanaryWords(buf->tail, FE_CANARY_WORDS, seed ^ 0xA5A5A5A5UL);
}

static void FeDiag_PrepareCanarySpec(FeCanaryBufSpec_t *buf, uint32_t seed)
{
  FeDiag_InitCanaryWords(buf->head, FE_CANARY_WORDS, seed);
  FeDiag_InitCanaryWords(buf->tail, FE_CANARY_WORDS, seed ^ 0xA5A5A5A5UL);
}

static void FeDiag_PrepareCanaryMfcc(FeCanaryBufMfcc_t *buf, uint32_t seed)
{
  FeDiag_InitCanaryWords(buf->head, FE_CANARY_WORDS, seed);
  FeDiag_InitCanaryWords(buf->tail, FE_CANARY_WORDS, seed ^ 0xA5A5A5A5UL);
}

static void FeDiag_PrintAlignOnce(const char *tag, const void *ptr)
{
#if APP_USC_FFT_CANARY_EN
  if (s_fe_canary_align_printed == 0U)
  {
    Debug_Print("[FE-CANARY] %s ptr=%p a4=%lu a8=%lu a16=%lu\r\n",
                tag,
                ptr,
                (unsigned long)(((uintptr_t)ptr) & 0x3UL),
                (unsigned long)(((uintptr_t)ptr) & 0x7UL),
                (unsigned long)(((uintptr_t)ptr) & 0xFUL));
  }
#else
  (void)tag;
  (void)ptr;
#endif
}

static void FeDiag_MarkAlignPrinted(void)
{
#if APP_USC_FFT_CANARY_EN
  s_fe_canary_align_printed = 1U;
#endif
}

static void FeDiag_CheckCanaryWords(const char *tag, const uint32_t *buf, uint32_t words, uint32_t seed)
{
#if APP_USC_FFT_CANARY_EN
  for (uint32_t i = 0U; i < words; i++)
  {
    uint32_t exp = FeDiag_CanaryWord(seed, i);
    if (buf[i] != exp)
    {
      if (s_fe_canary_prints < APP_USC_FFT_CANARY_PRINT_LIMIT)
      {
        Debug_Print("[FE-CANARY] CORRUPT %s[%lu] got=0x%08lx exp=0x%08lx\r\n",
                    tag,
                    (unsigned long)i,
                    (unsigned long)buf[i],
                    (unsigned long)exp);
      }
      s_fe_canary_prints++;
      break;
    }
  }
#else
  (void)tag;
  (void)buf;
  (void)words;
  (void)seed;
#endif
}

static uint8_t FeDiag_ShouldPrint(void)
{
#if APP_USC_MFCC_DEEP_DIAG_EN
  return (s_mfcc_deep_diag_active != 0U) ? 1U : 0U;
#else
  return 0U;
#endif
}

static void FeDiag_PrintStep(const char *tag)
{
#if APP_USC_MFCC_DEEP_DIAG_EN
  if (s_mfcc_deep_diag_active != 0U)
  {
    uint32_t now = AppDiag_DwtNow();
    Debug_Print("[DWT] %s cyc=%lu dus=%lu\r\n",
                tag,
                (unsigned long)now,
                (unsigned long)AppDiag_DwtDeltaUs(s_mfcc_deep_diag_base_cyc, now));
  }
#else
  (void)tag;
#endif
}

#ifndef M_PI
#define M_PI    3.14159265358979323846264338327950288 /*!< pi */
#endif

/**
 * @defgroup groupFeature Feature Extraction
 * @brief Spectral feature extraction functions
 * @{
 */

/**
 * @brief      Convert 16-bit PCM into floating point values
 *
 * @param      *pInSignal  points to input signal buffer
 * @param      *pOutSignal points to output signal buffer
 * @param      len         signal length
 */
void buf_to_float(int16_t *pInSignal, float32_t *pOutSignal, uint32_t len)
{
  for (uint32_t i = 0; i < len; i++)
  {
    pOutSignal[i] = (float32_t) pInSignal[i];
  }
}

/**
 * @brief      Convert 16-bit PCM into normalized floating point values
 *
 * @param      *pInSignal   points to input signal buffer
 * @param      *pOutSignal  points to output signal buffer
 * @param      len          signal length
 */
void buf_to_float_normed(int16_t *pInSignal, float32_t *pOutSignal, uint32_t len)
{
  for (uint32_t i = 0; i < len; i++)
  {
    pOutSignal[i] = (float32_t) pInSignal[i] / (1 << 15);
  }
}

/**
 * @brief      Power Spectrogram column
 *
 * @param      *S          points to an instance of the floating-point Spectrogram structure.
 * @param      *pInSignal  points to the in-place input signal frame of length FFTLen.
 * @param      *pOutCol    points to  output Spectrogram column.
 * @return     None
 */
void SpectrogramColumn(SpectrogramTypeDef *S, float32_t *pInSignal, float32_t *pOutCol)
{
  uint32_t frame_len = S->FrameLen;
  uint32_t n_fft = S->FFTLen;
  uint32_t n_spec = (n_fft / 2U) + 1U;
  float32_t *scratch_buffer = S->pScratch;
  float32_t *work_in = pInSignal;
  float32_t *work_fft = scratch_buffer;
  float32_t *work_out = pOutCol;
#if APP_USC_FFT_CANARY_EN
  uint8_t use_canary = 0U;
#endif
  float32_t first_energy;
  float32_t last_energy;

  FeDiag_PrintStep("spec_enter");

#if APP_USC_FFT_CANARY_EN
  if ((n_fft <= FE_CANARY_MAX_FFT) && (n_spec <= FE_CANARY_MAX_SPEC_OUT))
  {
    use_canary = 1U;
    FeDiag_PrepareCanaryFft(&s_fe_canary_in, 0x11110001UL);
    FeDiag_PrepareCanaryFft(&s_fe_canary_fft, 0x22220002UL);
    FeDiag_PrepareCanarySpec(&s_fe_canary_spec, 0x33330003UL);

    memset(s_fe_canary_in.data, 0, sizeof(s_fe_canary_in.data));
    memcpy(s_fe_canary_in.data, pInSignal, frame_len * sizeof(float32_t));
    work_in = s_fe_canary_in.data;
    work_fft = s_fe_canary_fft.data;
    work_out = s_fe_canary_spec.data;

    FeDiag_PrintAlignOnce("caller_in", pInSignal);
    FeDiag_PrintAlignOnce("caller_scratch", scratch_buffer);
    FeDiag_PrintAlignOnce("caller_out", pOutCol);
    FeDiag_PrintAlignOnce("guard_in", work_in);
    FeDiag_PrintAlignOnce("guard_fft", work_fft);
    FeDiag_PrintAlignOnce("guard_out", work_out);
    FeDiag_MarkAlignPrinted();
  }
#endif

  /* In-place window application (on signal length, not entire n_fft) */
  /* @note: OK to typecast because hannWin content is not modified */
  arm_mult_f32(work_in, S->pWindow, work_in, frame_len);
  FeDiag_PrintStep("spec_after_win");

  /* Zero pad if signal frame length is shorter than n_fft */
  memset(&work_in[frame_len], 0, (n_fft - frame_len) * sizeof(float32_t));
  FeDiag_PrintStep("spec_after_pad");

  /* FFT */
  FeDiag_PrintStep("spec_before_rfft");
  arm_rfft_fast_f32(S->pRfft, work_in, work_fft, 0);
  FeDiag_PrintStep("spec_after_rfft");

  /* Power spectrum */
  first_energy = work_fft[0] * work_fft[0];
  last_energy = work_fft[1] * work_fft[1];
  work_out[0] = first_energy;
  arm_cmplx_mag_squared_f32(&work_fft[2], &work_out[1], (n_fft / 2U) - 1U);
  work_out[n_fft / 2U] = last_energy;
  FeDiag_PrintStep("spec_after_mag2");

  /* Magnitude spectrum */
  if (S->Type == SPECTRUM_TYPE_MAGNITUDE)
  {
    for (uint32_t i = 0; i < n_spec; i++)
    {
      arm_sqrt_f32(work_out[i], &work_out[i]);
    }
    FeDiag_PrintStep("spec_after_sqrt");
  }

#if APP_USC_FFT_CANARY_EN
  if (use_canary != 0U)
  {
    FeDiag_CheckCanaryWords("in.head", s_fe_canary_in.head, FE_CANARY_WORDS, 0x11110001UL);
    FeDiag_CheckCanaryWords("in.tail", s_fe_canary_in.tail, FE_CANARY_WORDS, 0x11110001UL ^ 0xA5A5A5A5UL);
    FeDiag_CheckCanaryWords("fft.head", s_fe_canary_fft.head, FE_CANARY_WORDS, 0x22220002UL);
    FeDiag_CheckCanaryWords("fft.tail", s_fe_canary_fft.tail, FE_CANARY_WORDS, 0x22220002UL ^ 0xA5A5A5A5UL);
    FeDiag_CheckCanaryWords("spec.head", s_fe_canary_spec.head, FE_CANARY_WORDS, 0x33330003UL);
    FeDiag_CheckCanaryWords("spec.tail", s_fe_canary_spec.tail, FE_CANARY_WORDS, 0x33330003UL ^ 0xA5A5A5A5UL);
    memcpy(pOutCol, work_out, n_spec * sizeof(float32_t));
  }
#endif
}

/**
 * @brief      Mel Spectrogram column
 *
 * @param      *S          points to an instance of the floating-point Mel structure.
 * @param      *pInSignal  points to input signal frame of length FFTLen.
 * @param      *pOutCol    points to  output Mel Spectrogram column.
 * @return     None
 */
void MelSpectrogramColumn(MelSpectrogramTypeDef *S, float32_t *pInSignal, float32_t *pOutCol)
{
  float32_t *tmp_buffer = S->SpectrogramConf->pScratch;

  /* Power Spectrogram */
  SpectrogramColumn(S->SpectrogramConf, pInSignal, tmp_buffer);

  /* Mel Filter Banks Application */
  MelFilterbank(S->MelFilter, tmp_buffer, pOutCol);
}

/**
 * @brief      Log-Mel Spectrogram column
 *
 * @param      *S          points to an instance of the floating-point Log-Mel structure.
 * @param      *pInSignal  points to input signal frame of length FFTLen.
 * @param      *pOutCol    points to  output Log-Mel Spectrogram column.
 * @return     None
 */
void LogMelSpectrogramColumn(LogMelSpectrogramTypeDef *S, float32_t *pInSignal, float32_t *pOutCol)
{
  uint32_t n_mels = S->MelSpectrogramConf->MelFilter->NumMels;
  float32_t top_dB = S->TopdB;
  float32_t ref = S->Ref;
  float32_t *tmp_buffer = S->MelSpectrogramConf->SpectrogramConf->pScratch;

  FeDiag_PrintStep("logmel_enter");
  SpectrogramColumn(S->MelSpectrogramConf->SpectrogramConf, pInSignal, tmp_buffer);
  FeDiag_PrintStep("logmel_after_spec");

  /* Mel Filter Banks Application to power spectrum column */
  MelFilterbank(S->MelSpectrogramConf->MelFilter, tmp_buffer, pOutCol);
  FeDiag_PrintStep("logmel_after_mel");

  /* Scaling */
  for (uint32_t i = 0; i < n_mels; i++) {
    pOutCol[i] /= ref;
  }

  /* Avoid log of zero or a negative number */
  for (uint32_t i = 0; i < n_mels; i++) {
    if (pOutCol[i] <= 0.0f) {
      pOutCol[i] = FLT_MIN;
    }
  }

  if (S->LogFormula == LOGMELSPECTROGRAM_SCALE_DB)
  {
    /* Convert power spectrogram to decibel */
    for (uint32_t i = 0; i < n_mels; i++) {
      pOutCol[i] = 10.0f * log10f(pOutCol[i]);
    }
    FeDiag_PrintStep("logmel_after_log10");

    /* Threshold output to -top_dB dB */
    for (uint32_t i = 0; i < n_mels; i++) {
      pOutCol[i] = (pOutCol[i] < -top_dB) ? (-top_dB) : (pOutCol[i]);
    }
  }
  else
  {
    /* Convert power spectrogram to log scale */
    for (uint32_t i = 0; i < n_mels; i++) {
      pOutCol[i] = logf(pOutCol[i]);
    }
    FeDiag_PrintStep("logmel_after_logf");
  }

}

/**
 * @brief      Mel-Frequency Cepstral Coefficients (MFCCs) column
 *
 * @param      *S          points to an instance of the floating-point MFCC structure.
 * @param      *pInSignal  points to input signal frame of length FFTLen.
 * @param      *pOutCol    points to  output MFCC spectrogram column.
 * @return     None
 */
void MfccColumn(MfccTypeDef *S, float32_t *pInSignal, float32_t *pOutCol)
{
  float32_t *tmp_buffer = S->pScratch;
  float32_t *work_tmp = tmp_buffer;
  float32_t *work_out = pOutCol;
#if APP_USC_FFT_CANARY_EN
  uint8_t use_canary = 0U;
#endif

#if APP_USC_MFCC_DEEP_DIAG_EN
  if (s_mfcc_deep_diag_count < APP_USC_MFCC_DEEP_DIAG_LIMIT)
  {
    s_mfcc_deep_diag_active = 1U;
    s_mfcc_deep_diag_base_cyc = AppDiag_DwtNow();
    Debug_Print("[DWT] mfcc_in cyc=%lu pIn=%p pOut=%p scratch=%p\r\n",
                (unsigned long)s_mfcc_deep_diag_base_cyc,
                (void *)pInSignal,
                (void *)pOutCol,
                (void *)tmp_buffer);
  }
  else
  {
    s_mfcc_deep_diag_active = 0U;
  }
#endif

#if APP_USC_FFT_CANARY_EN
  if ((S->LogMelConf->MelSpectrogramConf->MelFilter->NumMels <= FE_CANARY_MAX_MFCC_TMP) &&
      (S->NumMfccCoefs <= FE_CANARY_MAX_MFCC_TMP))
  {
    use_canary = 1U;
    FeDiag_PrepareCanaryMfcc(&s_fe_canary_mfcc_tmp, 0x44440004UL);
    FeDiag_PrepareCanaryMfcc(&s_fe_canary_mfcc_out, 0x55550005UL);
    work_tmp = s_fe_canary_mfcc_tmp.data;
    work_out = s_fe_canary_mfcc_out.data;
  }
#endif

  FeDiag_PrintStep("mfcc_before_logmel");
  LogMelSpectrogramColumn(S->LogMelConf, pInSignal, work_tmp);
  FeDiag_PrintStep("mfcc_after_logmel");

  /* DCT for computing MFCCs from spectrogram slice. */
  FeDiag_PrintStep("mfcc_before_dct");
  DCT(S->pDCT, work_tmp, work_out);
  FeDiag_PrintStep("mfcc_after_dct");

#if APP_USC_FFT_CANARY_EN
  if (use_canary != 0U)
  {
    FeDiag_CheckCanaryWords("mfccTmp.head", s_fe_canary_mfcc_tmp.head, FE_CANARY_WORDS, 0x44440004UL);
    FeDiag_CheckCanaryWords("mfccTmp.tail", s_fe_canary_mfcc_tmp.tail, FE_CANARY_WORDS, 0x44440004UL ^ 0xA5A5A5A5UL);
    FeDiag_CheckCanaryWords("mfccOut.head", s_fe_canary_mfcc_out.head, FE_CANARY_WORDS, 0x55550005UL);
    FeDiag_CheckCanaryWords("mfccOut.tail", s_fe_canary_mfcc_out.tail, FE_CANARY_WORDS, 0x55550005UL ^ 0xA5A5A5A5UL);
    memcpy(pOutCol, work_out, S->NumMfccCoefs * sizeof(float32_t));
  }
#endif

#if APP_USC_MFCC_DEEP_DIAG_EN
  if (s_mfcc_deep_diag_active != 0U)
  {
    s_mfcc_deep_diag_count++;
    s_mfcc_deep_diag_active = 0U;
  }
#endif
}

/**
 * @} end of groupFeature
 */

#if defined(__GNUC__)
#pragma GCC pop_options
#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
