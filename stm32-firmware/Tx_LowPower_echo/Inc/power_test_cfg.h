#ifndef POWER_TEST_CFG_H
#define POWER_TEST_CFG_H

#include "stm32u5xx_hal.h"

#define APP_RUN_MODE_DATA_CAPTURE       0
#define APP_RUN_MODE_RECOGNITION        1
#define APP_RUN_MODE                    APP_RUN_MODE_RECOGNITION
//#define APP_RUN_MODE                  APP_RUN_MODE_DATA_CAPTURE
/* Set APP_RUN_MODE_DATA_CAPTURE to align UART stream/handshake with main.py. */

/* ============================================================
 * Power/profile selection
 *   A: 只采样，不跑模型（进一步下压基线：2 MHz + 更大 DMA chunk）
 *   B: 只跑 NEAI，不跑 USC，不打印（NEAI 保持低频）
 *   C: 跑 NEAI + USC，关闭识别打印（NEAI 低频，只有 USC 升频）
 *   D: 完整逻辑对照（NEAI 低频，只有 USC 升频 + 打印）
 * ============================================================ */
#define APP_PWR_MODE_A_SAMPLING_ONLY      1
#define APP_PWR_MODE_B_NEAI_ONLY          2
#define APP_PWR_MODE_C_NEAI_USC_SILENT    3
#define APP_PWR_MODE_D_FULL               4

#ifndef APP_PWR_PROFILE_MODE
#define APP_PWR_PROFILE_MODE              APP_PWR_MODE_C_NEAI_USC_SILENT
//#define APP_PWR_PROFILE_MODE              APP_PWR_MODE_B_NEAI_ONLY
#endif

#define AUDIO_ACQ_MODE_LPTIM_TRIGGER    0
#define AUDIO_ACQ_MODE_ADC_CONTINUOUS   1
#define AUDIO_ACQ_MODE_SEL              AUDIO_ACQ_MODE_LPTIM_TRIGGER

#define AUDIO_ACQ_LPTIM_PB2_EXPORT_OFF  0
#define AUDIO_ACQ_LPTIM_PB2_EXPORT_ON   1
#define AUDIO_ACQ_LPTIM_PB2_EXPORT      AUDIO_ACQ_LPTIM_PB2_EXPORT_OFF

#define APP_AUDIO_STAGE_RAW_DMA_ONLY     0
#define APP_AUDIO_STAGE_RING_WINDOW_ONLY 1
#define APP_AUDIO_STAGE_FULL_NEAI        2
#define APP_AUDIO_STAGE_FULL_USC         3

#if   (APP_PWR_PROFILE_MODE == APP_PWR_MODE_A_SAMPLING_ONLY)
  #define APP_PROFILE_ENABLE_NEAI         0
  #define APP_PROFILE_ENABLE_USC          0
  #define APP_AUDIO_STAGE_SEL             APP_AUDIO_STAGE_RAW_DMA_ONLY
  #define APP_SYSCLK_BOOT_MHZ             2U
  #define APP_SYSCLK_DYNAMIC_NEAI_BOOST   0
  #define APP_SYSCLK_DYNAMIC_USC_BOOST    0
  #define APP_DMA_CHUNK_SAMPLES_PROFILE   1024U
#elif (APP_PWR_PROFILE_MODE == APP_PWR_MODE_B_NEAI_ONLY)
  #define APP_PROFILE_ENABLE_NEAI         1
  #define APP_PROFILE_ENABLE_USC          0
  #define APP_AUDIO_STAGE_SEL             APP_AUDIO_STAGE_FULL_NEAI
  #define APP_SYSCLK_BOOT_MHZ             16U
  #define APP_SYSCLK_DYNAMIC_NEAI_BOOST   0
  #define APP_SYSCLK_DYNAMIC_USC_BOOST    0
  #define APP_DMA_CHUNK_SAMPLES_PROFILE   256U
#elif (APP_PWR_PROFILE_MODE == APP_PWR_MODE_C_NEAI_USC_SILENT)
  #define APP_PROFILE_ENABLE_NEAI         1
  #define APP_PROFILE_ENABLE_USC          1
  #define APP_AUDIO_STAGE_SEL             APP_AUDIO_STAGE_FULL_USC
  #define APP_SYSCLK_BOOT_MHZ             16U
  #define APP_SYSCLK_DYNAMIC_NEAI_BOOST   0
  #define APP_SYSCLK_DYNAMIC_USC_BOOST    1
  #define APP_DMA_CHUNK_SAMPLES_PROFILE   256U
#else /* APP_PWR_MODE_D_FULL */
  #define APP_PROFILE_ENABLE_NEAI         1
  #define APP_PROFILE_ENABLE_USC          1
  #define APP_AUDIO_STAGE_SEL             APP_AUDIO_STAGE_FULL_USC
  #define APP_SYSCLK_BOOT_MHZ             16U
  #define APP_SYSCLK_DYNAMIC_NEAI_BOOST   0
  #define APP_SYSCLK_DYNAMIC_USC_BOOST    1
  #define APP_DMA_CHUNK_SAMPLES_PROFILE   256U
#endif

#if ((APP_SYSCLK_DYNAMIC_NEAI_BOOST != 0) || (APP_SYSCLK_DYNAMIC_USC_BOOST != 0))
  #define APP_SYSCLK_DYNAMIC_INFER_BOOST  1
#else
  #define APP_SYSCLK_DYNAMIC_INFER_BOOST  0
#endif

#define APP_SYSCLK_LOW_MHZ             APP_SYSCLK_BOOT_MHZ
#define APP_SYSCLK_HIGH_MHZ            160U

#if (APP_RUN_MODE == APP_RUN_MODE_DATA_CAPTURE)
  #define APP_THREADX_IDLE_WFI_ENABLE  0
#else
  #define APP_THREADX_IDLE_WFI_ENABLE  1
#endif

#define APP_TIMING_LOG_ENABLE          0
#define APP_TIMING_PULSE_ENABLE        0
#define APP_TIMING_PULSE_WIDTH_US      2U
#define APP_TIMING_PULSE_GAP_US        20U
#define APP_ACCURACY_LOG_ENABLE        0

//#if (APP_RUN_MODE == APP_RUN_MODE_DATA_CAPTURE)
//  #define APP_DEBUG_UART_ON            1
//#else
//  #if ((APP_TIMING_LOG_ENABLE != 0) || (APP_ACCURACY_LOG_ENABLE != 0) || (APP_PWR_PROFILE_MODE == APP_PWR_MODE_D_FULL))
//    #define APP_DEBUG_UART_ON          1
//  #else
//    #define APP_DEBUG_UART_ON          0
//  #endif
//#endif



/* Heartbeat/diagnostic periodic logs in app_threadx.c.
 * Keep disabled for current profiling work to avoid periodic wakeups. */
#define APP_THREADX_HEARTBEAT_LOG_EN    0

/* app_threadx.c log class switches.
 * Default: keep only jump-related logs and recognition-result logs.
 * Do not delete code paths; toggle here when needed. */
#define APP_THREADX_LOG_JUMP_EN         0
#define APP_THREADX_LOG_RECOG_EN        0
#define APP_THREADX_LOG_INIT_EN         0
#define APP_THREADX_LOG_TIMING_EN       0
#define APP_THREADX_LOG_DIAG_EN         0
#define APP_THREADX_LOG_NEAI_EN         0
#define APP_THREADX_LOG_MISC_EN         0


#if (APP_RUN_MODE == APP_RUN_MODE_DATA_CAPTURE)
  #define APP_DEBUG_UART_ON 1
#else
  #if ((APP_TIMING_LOG_ENABLE != 0) || \
       (APP_ACCURACY_LOG_ENABLE != 0) || \
       (APP_PWR_PROFILE_MODE == APP_PWR_MODE_D_FULL) || \
       (APP_THREADX_LOG_RECOG_EN != 0) || \
       (APP_THREADX_LOG_JUMP_EN != 0))
    #define APP_DEBUG_UART_ON 1
  #else
    #define APP_DEBUG_UART_ON 0
  #endif
#endif


/* Non-app_threadx source file log switches.
 * Set to 0 to compile out prints from the corresponding module. */
#define APP_MAIN_BOOT_LOG_EN          0
#define APP_AUDIO_ACQ_LOG_EN          0
#define APP_USC_AI_LOG_EN             0

/* NEAI hotword gate (echo -> USC) */
#define APP_NEAI_GATE_ENABLE            APP_PROFILE_ENABLE_NEAI
#define APP_NEAI_HOTWORD_NAME           "echo"
#define APP_NEAI_WAKE_SCORE_THRESHOLD   200U
#define APP_NEAI_WAKE_MARGIN_THRESHOLD  0U
#define APP_NEAI_WAKE_HITS              2U
#define APP_USC_WAKE_MAX_WINDOWS        24U
#define APP_USC_WAKE_MAX_INFER_FRAMES   APP_USC_WAKE_MAX_WINDOWS
#define APP_USC_SEND_AT_ON_ACCEPT       1
#define APP_HAS_HUART2                  1
#define APP_NEAI_MAX_WINDOWS_PER_PASS      1U
#define APP_NEAI_DENSE_ENTER_SCORE         100U  /* 10% */
#define APP_NEAI_DENSE_EXIT_SCORE          50U   /* 5% */
#define APP_NEAI_QUIET_HOP_MULTIPLIER      2U
#define APP_NEAI_DENSE_HOP_MULTIPLIER      1U

/* USC command accept tuning.
 * Relax these if you want AT sending to trigger more easily after a valid command. */
//#define APP_USC_ENTER_THRESHOLD         30U
//#define APP_USC_ACCEPT_THRESHOLD        35U
//#define APP_USC_ACCEPT_HIT_SCORE        30U
//#define APP_USC_ACCEPT_MARGIN           4U
//#define APP_USC_ACCEPT_MARGIN_SCORE     35U
//#define APP_USC_HOLD_FRAMES             3U
//#define APP_USC_MIN_HITS                2U
//#define APP_USC_SWITCH_DELTA            6U
//#define APP_USC_BG_PREVIEW_THRESHOLD    12U
//#define APP_USC_ACCEPT_COOLDOWN_FRAMES  4U
//#define APP_USC_SUPPRESS_FRAMES         4U
//#define APP_USC_REARM_QUIET_FRAMES      8U
//#define APP_USC_REARM_SCORE_MAX         45U

#define APP_USC_ENTER_THRESHOLD         20U
#define APP_USC_ACCEPT_THRESHOLD        20U
#define APP_USC_ACCEPT_HIT_SCORE        20U
#define APP_USC_ACCEPT_MARGIN           0U
#define APP_USC_ACCEPT_MARGIN_SCORE     20U
#define APP_USC_HOLD_FRAMES             2U
#define APP_USC_MIN_HITS                1U
#define APP_USC_SWITCH_DELTA            2U
#define APP_USC_BG_PREVIEW_THRESHOLD    8U
#define APP_USC_ACCEPT_COOLDOWN_FRAMES  1U
#define APP_USC_SUPPRESS_FRAMES         0U
#define APP_USC_REARM_QUIET_FRAMES      1U
#define APP_USC_REARM_SCORE_MAX         100U


/* After AT is sent: 0 = keep USC mode latched, 1 = return to NEAI mode. */
#define APP_USC_RETURN_TO_NEAI_AFTER_AT 1

/* Stage mark GPIO when entering USC.
 * 0 = disabled, 1 = drive the stage-mark GPIO. */
#define APP_STAGE_MARK_ENABLE           0

#define APP_HSITRIM_14                  14U
#define APP_HSITRIM_15                  15U
#define APP_HSITRIM_VALUE               APP_HSITRIM_15

#define APP_AUDIO_ADC_SRC_MSIK         0
#define APP_AUDIO_ADC_SRC_PLL2         1
#define APP_AUDIO_ADC_SRC_SEL          APP_AUDIO_ADC_SRC_MSIK
#define APP_AUDIO_ADC_FREQ_HZ          2000000UL
#define APP_AUDIO_MSIK_RANGE           RCC_MSIKRANGE_5



#endif
