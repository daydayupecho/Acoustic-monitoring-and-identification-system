#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

#include "usc_network.h"
#include "power_test_cfg.h"
#include "NanoEdgeAI.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_SAMPLE_RATE_HZ            (16000U)
/* A 模式把 DMA chunk 加大，减少 CPU 被 DMA 半传输/全传输唤醒的频率。 */
#define AUDIO_DMA_CHUNK_SAMPLES         (APP_DMA_CHUNK_SAMPLES_PROFILE)
#define AUDIO_DMA_BUFFER_SAMPLES        (AUDIO_DMA_CHUNK_SAMPLES * 2U)

/* Increased to give the preproc thread more time before old raw windows age out. */
#define AUDIO_RINGBUF_SAMPLES           (65536U)

/* NEAI stage window (existing path kept unchanged). */
//#define AUDIO_NEAI_FRAME_SAMPLES        (10240U)
#define AUDIO_NEAI_FRAME_SAMPLES  ((uint32_t)NEAI_INPUT_SIGNAL_LENGTH)
//#define AUDIO_NEAI_HOP_SAMPLES          (AUDIO_NEAI_FRAME_SAMPLES / 2U)
#define AUDIO_NEAI_HOP_SAMPLES          (AUDIO_NEAI_FRAME_SAMPLES / 4U)
//#define AUDIO_NEAI_HOP_SAMPLES          (AUDIO_NEAI_FRAME_SAMPLES / 8U)
//#define AUDIO_NEAI_HOP_SAMPLES    (AUDIO_NEAI_FRAME_SAMPLES)

/* USC front-end configuration.
 * The model input shape is taken directly from usc_network.h so that replacing
 * the USC model automatically updates time steps / feature count.
 * FRAME_LEN and STRIDE_COLS still describe the MFCC front-end itself and must
 * match the model training pipeline. */
#define AUDIO_USC_FFT_LEN               (512U)
#define AUDIO_USC_FRAME_LEN             (512U)
#define AUDIO_USC_NUM_TIME_STEPS        AI_USC_NETWORK_IN_1_HEIGHT
#define AUDIO_USC_NUM_MFCC              (AI_USC_NETWORK_IN_1_WIDTH * AI_USC_NETWORK_IN_1_CHANNEL)
#define AUDIO_USC_FEATURE_COUNT         AI_USC_NETWORK_IN_1_SIZE
#define AUDIO_USC_STRIDE_COLS           (4U)

/* For 512-sample columns, total frame = first 512 + (N-1) * 512. */
#define AUDIO_USC_FRAME_SAMPLES   (AUDIO_USC_FRAME_LEN + ((AUDIO_USC_NUM_TIME_STEPS - 1U) * AUDIO_USC_FRAME_LEN))

#define AUDIO_USC_HOP_SAMPLES   (AUDIO_USC_STRIDE_COLS * AUDIO_USC_FRAME_LEN)

#if (AUDIO_USC_FEATURE_COUNT != (AUDIO_USC_NUM_TIME_STEPS * AUDIO_USC_NUM_MFCC))
#error "USC model input size mismatch with derived time-step/MFCC dimensions"
#endif

#define AUDIO_DEBUG_HEARTBEAT_MS        (5000U)

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_CONFIG_H */
