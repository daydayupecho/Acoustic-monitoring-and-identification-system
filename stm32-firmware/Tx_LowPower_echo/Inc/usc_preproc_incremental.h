#ifndef USC_PREPROC_INCREMENTAL_H
#define USC_PREPROC_INCREMENTAL_H

#include <stdint.h>
#include <string.h>
#include "audio_window.h"
#include "audio_config.h"
#include "usc_preproc.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef USC_PREPROC_INC_MAX_FEATURES
#define USC_PREPROC_INC_MAX_FEATURES   (AUDIO_USC_NUM_TIME_STEPS * AUDIO_USC_NUM_MFCC)
#endif

/* Skeleton status codes */
typedef enum
{
  USC_PREINC_OK = 0,
  USC_PREINC_ERR_ARG = -1,
  USC_PREINC_ERR_PREPROC = -2,
  USC_PREINC_ERR_CONFIG = -3
} UscPreprocInc_Status_t;

typedef enum
{
  USC_PREINC_MODE_FULL_BOOT = 0,        /* first frame full preprocess */
  USC_PREINC_MODE_FULL_RESET = 1,       /* discontinuity/reset => full preprocess */
  USC_PREINC_MODE_CONTIG_FALLBACK = 2,  /* contiguous hop, but still full preprocess fallback */
  USC_PREINC_MODE_CONTIG_INC = 3        /* contiguous hop, true incremental path (future) */
} UscPreprocInc_Mode_t;

typedef struct
{
  uint8_t  valid;
  uint32_t frame_samples;
  uint32_t hop_samples;
  uint32_t feature_count;
  uint32_t cols_total;
  uint32_t cols_per_hop;
  uint32_t coeffs_per_col;
  uint32_t last_start_index;

  /* Cached full tensor from previous accepted frame.
   * For now this is only used to establish the skeleton state machine.
   * Future work: update only the last cols_per_hop columns instead of
   * recomputing the whole tensor. */
  float feature_cache[USC_PREPROC_INC_MAX_FEATURES];
} UscPreprocIncremental_t;

void UscPreprocIncremental_Reset(UscPreprocIncremental_t *ctx);

UscPreprocInc_Status_t UscPreprocIncremental_Init(UscPreprocIncremental_t *ctx,
                                                  uint32_t frame_samples,
                                                  uint32_t hop_samples,
                                                  uint32_t cols_total,
                                                  uint32_t coeffs_per_col,
                                                  uint32_t cols_per_hop);

UscPreprocInc_Status_t UscPreprocIncremental_ProcessView(UscPreprocIncremental_t *ctx,
                                                         const AudioWindow_View_t *view,
                                                         float *feat_out,
                                                         UscPreprocInc_Mode_t *mode_out,
                                                         uint32_t *new_cols_out);

#ifdef __cplusplus
}
#endif

#endif /* USC_PREPROC_INCREMENTAL_H */
