#include "usc_preproc_incremental.h"

static UscPreprocInc_Status_t UscPreprocInc_RunFull(UscPreprocIncremental_t *ctx,
                                                    const AudioWindow_View_t *view,
                                                    float *feat_out,
                                                    UscPreprocInc_Mode_t mode,
                                                    UscPreprocInc_Mode_t *mode_out,
                                                    uint32_t *new_cols_out)
{
  if (UscPreproc_ProcessFrameView(view, feat_out) != 0)
  {
    return USC_PREINC_ERR_PREPROC;
  }

  if (ctx->feature_count <= USC_PREPROC_INC_MAX_FEATURES)
  {
    memcpy(ctx->feature_cache, feat_out, ctx->feature_count * sizeof(float));
  }

  ctx->valid = 1U;
  ctx->last_start_index = view->start_index;

  if (mode_out != 0)
  {
    *mode_out = mode;
  }
  if (new_cols_out != 0)
  {
    *new_cols_out = ctx->cols_total;
  }
  return USC_PREINC_OK;
}

void UscPreprocIncremental_Reset(UscPreprocIncremental_t *ctx)
{
  if (ctx == 0)
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

  if ((ctx == 0) || (frame_samples == 0U) || (hop_samples == 0U) ||
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

  if ((ctx == 0) || (view == 0) || (feat_out == 0))
  {
    return USC_PREINC_ERR_ARG;
  }

  /* Future true-incremental version should:
   * 1) detect contiguous hop progression,
   * 2) compute only cols_per_hop new MFCC columns,
   * 3) shift the cached tensor left by cols_per_hop columns,
   * 4) append new columns,
   * 5) export the refreshed 52x32 tensor.
   *
   * This skeleton keeps the state machine and fallbacks compile-safe while
   * still routing all current cases through the known-good full preprocess.
   */

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

  /* Contiguous hop: shift cached tensor left and append only the new columns. */
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

    if (mode_out != 0)
    {
      *mode_out = USC_PREINC_MODE_CONTIG_INC;
    }
    if (new_cols_out != 0)
    {
      *new_cols_out = shift_cols;
    }
    return USC_PREINC_OK;
  }
}
