/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @brief   ThreadX applicative file
  ******************************************************************************
  */
/* USER CODE END Header */

#include "app_threadx.h"
#include "main.h"
#include "debug_uart.h"
#include "audio_acq.h"
#include "audio_ringbuf.h"
#include "audio_window.h"
#include "perf_counter.h"
#include "usc_preproc.h"
#include "usc_preproc_incremental.h"
#include "usc_ai_runner.h"
#include "usc_network.h"
#include "NanoEdgeAI.h"
#include <string.h>

#define AUDIO_DIAG_PERIOD_TICKS     100U
#if (APP_THREADX_HEARTBEAT_LOG_EN != 0)
#define AUDIO_EVENT_TIMEOUT_TICKS   AUDIO_DIAG_PERIOD_TICKS
#else
#define AUDIO_EVENT_TIMEOUT_TICKS   TX_WAIT_FOREVER
#endif
#define USC_FRAME_SAMPLES           AUDIO_USC_FRAME_SAMPLES
#define USC_HOP_SAMPLES             AUDIO_USC_HOP_SAMPLES
#define USC_FEATURE_COUNT           AUDIO_USC_FEATURE_COUNT
#define USC_COLS_TOTAL              AUDIO_USC_NUM_TIME_STEPS
#define USC_COEFFS_PER_COL          AUDIO_USC_NUM_MFCC
#define USC_COLS_PER_HOP            AUDIO_USC_STRIDE_COLS
#define BC_HOP_BUDGET_MS            128U
#define BC_HOP_BUDGET_US            (BC_HOP_BUDGET_MS * 1000U)

#if (APP_THREADX_LOG_JUMP_EN != 0)
#define TXLP5_LOG_JUMP_PRINT(...)    Debug_Print(__VA_ARGS__)
#define TXLP5_LOG_JUMP_PUTS(s)       Debug_PutString(s)
#else
#define TXLP5_LOG_JUMP_PRINT(...)    do { } while (0)
#define TXLP5_LOG_JUMP_PUTS(s)       do { (void)(s); } while (0)
#endif

#if (APP_THREADX_LOG_RECOG_EN != 0)
#define TXLP5_LOG_RECOG_PRINT(...)   Debug_Print(__VA_ARGS__)
#define TXLP5_LOG_RECOG_PUTS(s)      Debug_PutString(s)
#else
#define TXLP5_LOG_RECOG_PRINT(...)   do { } while (0)
#define TXLP5_LOG_RECOG_PUTS(s)      do { (void)(s); } while (0)
#endif

#if (APP_THREADX_LOG_INIT_EN != 0)
#define TXLP5_LOG_INIT_PRINT(...)    Debug_Print(__VA_ARGS__)
#define TXLP5_LOG_INIT_PUTS(s)       Debug_PutString(s)
#else
#define TXLP5_LOG_INIT_PRINT(...)    do { } while (0)
#define TXLP5_LOG_INIT_PUTS(s)       do { (void)(s); } while (0)
#endif

#if (APP_THREADX_LOG_TIMING_EN != 0)
#define TXLP5_LOG_TIMING_PRINT(...)  Debug_Print(__VA_ARGS__)
#define TXLP5_LOG_TIMING_PUTS(s)     Debug_PutString(s)
#else
#define TXLP5_LOG_TIMING_PRINT(...)  do { } while (0)
#define TXLP5_LOG_TIMING_PUTS(s)     do { (void)(s); } while (0)
#endif

#if (APP_THREADX_LOG_DIAG_EN != 0)
#define TXLP5_LOG_DIAG_PRINT(...)    Debug_Print(__VA_ARGS__)
#define TXLP5_LOG_DIAG_PUTS(s)       Debug_PutString(s)
#else
#define TXLP5_LOG_DIAG_PRINT(...)    do { } while (0)
#define TXLP5_LOG_DIAG_PUTS(s)       do { (void)(s); } while (0)
#endif

#if (APP_THREADX_LOG_NEAI_EN != 0)
#define TXLP5_LOG_NEAI_PRINT(...)    Debug_Print(__VA_ARGS__)
#define TXLP5_LOG_NEAI_PUTS(s)       Debug_PutString(s)
#else
#define TXLP5_LOG_NEAI_PRINT(...)    do { } while (0)
#define TXLP5_LOG_NEAI_PUTS(s)       do { (void)(s); } while (0)
#endif

#if (APP_THREADX_LOG_MISC_EN != 0)
#define TXLP5_LOG_MISC_PRINT(...)    Debug_Print(__VA_ARGS__)
#define TXLP5_LOG_MISC_PUTS(s)       Debug_PutString(s)
#else
#define TXLP5_LOG_MISC_PRINT(...)    do { } while (0)
#define TXLP5_LOG_MISC_PUTS(s)       do { (void)(s); } while (0)
#endif

#if (APP_THREADX_LOG_USCAI_EN != 0)
#define TXLP5_LOG_USCAI_PRINT(...)   Debug_Print(__VA_ARGS__)
#define TXLP5_LOG_USCAI_PUTS(s)      Debug_PutString(s)
#else
#define TXLP5_LOG_USCAI_PRINT(...)   do { } while (0)
#define TXLP5_LOG_USCAI_PUTS(s)      do { (void)(s); } while (0)
#endif

/* USC command decision (candidate-hold)
 * Thresholds are configured from power_test_cfg.h so they can be tuned without
 * editing this source file every time. */
#define USC_ENTER_THRESHOLD         APP_USC_ENTER_THRESHOLD
#define USC_ACCEPT_THRESHOLD        APP_USC_ACCEPT_THRESHOLD
#define USC_ACCEPT_HIT_SCORE        APP_USC_ACCEPT_HIT_SCORE
#define USC_ACCEPT_MARGIN           APP_USC_ACCEPT_MARGIN
#define USC_ACCEPT_MARGIN_SCORE     APP_USC_ACCEPT_MARGIN_SCORE
#define USC_HOLD_FRAMES             APP_USC_HOLD_FRAMES
#define USC_MIN_HITS                APP_USC_MIN_HITS
#define USC_SWITCH_DELTA            APP_USC_SWITCH_DELTA
#define USC_BG_PREVIEW_THRESHOLD    APP_USC_BG_PREVIEW_THRESHOLD
#define USC_ACCEPT_COOLDOWN_FRAMES  APP_USC_ACCEPT_COOLDOWN_FRAMES
#define USC_SUPPRESS_FRAMES         APP_USC_SUPPRESS_FRAMES
#define USC_REARM_QUIET_FRAMES      APP_USC_REARM_QUIET_FRAMES
#define USC_REARM_SCORE_MAX         APP_USC_REARM_SCORE_MAX
#define USC_BG_CLASS                (AI_USC_NETWORK_OUT_1_SIZE - 1U)
#define USC_INVALID_CLASS           (AI_USC_NETWORK_OUT_1_SIZE)

/* NEAI hotword gate */
#define NEAI_INPUT_COUNT              (NEAI_INPUT_SIGNAL_LENGTH * NEAI_INPUT_AXIS_NUMBER)
#define NEAI_OTHER_NAME_1             "other"
#define NEAI_OTHER_NAME_2             "unknown"
#define NEAI_OTHER_NAME_3             "background"

#define NEAI_MAX_WINDOWS_PER_PASS       APP_NEAI_MAX_WINDOWS_PER_PASS
#define NEAI_DENSE_ENTER_SCORE          APP_NEAI_DENSE_ENTER_SCORE
#define NEAI_DENSE_EXIT_SCORE           APP_NEAI_DENSE_EXIT_SCORE
#define NEAI_QUIET_HOP_MULTIPLIER       APP_NEAI_QUIET_HOP_MULTIPLIER
#define NEAI_DENSE_HOP_MULTIPLIER       APP_NEAI_DENSE_HOP_MULTIPLIER

#if defined(__GNUC__)
#define APP_ALIGN16 __attribute__((aligned(16)))
#else
#define APP_ALIGN16
#endif

TX_THREAD tx_app_thread;
TX_THREAD tx_preproc_thread;
TX_THREAD tx_ai_thread;
TX_THREAD tx_at_thread;

#if (APP_HAS_HUART2 != 0)
UART_HandleTypeDef huart2;
static volatile uint8_t s_at_uart_ready = 0U;
static void AtUart_EnableHSI(void);
static void MX_USART2_UART_Init(void);
static uint8_t AtUart_EnsureReady(void);
#endif

static TX_SEMAPHORE tx_audio_sem;
static TX_SEMAPHORE tx_feat_free_sem;
static volatile uint8_t s_audio_sem_ready = 0U;

static TX_QUEUE tx_raw_queue;
static TX_QUEUE tx_feat_queue;
static TX_QUEUE tx_free_feat_queue;
static TX_QUEUE tx_at_queue;

static ULONG tx_raw_queue_storage[RAW_QUEUE_DEPTH];
static ULONG tx_feat_queue_storage[FEAT_QUEUE_DEPTH];
static ULONG tx_free_feat_queue_storage[FEAT_QUEUE_DEPTH];
static ULONG tx_at_queue_storage[AT_QUEUE_DEPTH];

static APP_ALIGN16 float s_feat_slots[FEAT_QUEUE_DEPTH][USC_FEATURE_COUNT];
static volatile uint32_t s_feat_slot_start_idx[FEAT_QUEUE_DEPTH];

static volatile uint32_t s_raw_sent  = 0U;
static volatile uint32_t s_raw_recv  = 0U;
static volatile uint32_t s_raw_drop  = 0U;
static volatile uint32_t s_feat_sent = 0U;
static volatile uint32_t s_feat_recv = 0U;
static volatile uint32_t s_feat_drop = 0U;
static volatile uint32_t s_usc_infer_count = 0U;
static volatile uint32_t s_usc_jump_count = 0U;
static volatile uint32_t s_usc_last_start_idx = 0U;
static volatile uint32_t s_usc_last_tick = 0U;
static volatile uint32_t s_usc_last_dt_ms = 0U;
static volatile uint32_t s_usc_last_dstart = 0U;
static volatile uint32_t s_c_done    = 0U;
static volatile uint32_t s_pre_ok    = 0U;
static volatile uint32_t s_pre_err   = 0U;
static volatile uint32_t s_view_err  = 0U;
static volatile uint32_t s_ai_ok     = 0U;
static volatile uint32_t s_ai_err    = 0U;
static volatile uint32_t s_ai_top1   = 0U;
static volatile uint32_t s_ai_top2   = 0U;
static volatile uint32_t s_ai_top1_p = 0U;
static volatile uint32_t s_ai_top2_p = 0U;
static volatile uint32_t s_pre_boot  = 0U;
static volatile uint32_t s_pre_reset = 0U;
static volatile uint32_t s_pre_fallback = 0U;
static volatile uint32_t s_pre_inc = 0U;
static volatile uint32_t s_pre_inc_cols = 0U;

static volatile uint32_t s_b_time_count = 0U;
static volatile uint32_t s_b_time_last_us = 0U;
static volatile uint32_t s_b_time_max_us = 0U;
static volatile uint32_t s_b_time_over_budget = 0U;
static volatile uint32_t s_b_first_over_start_idx = 0U;
static volatile uint32_t s_b_first_over_us = 0U;
static volatile uint32_t s_c_time_count = 0U;
static volatile uint32_t s_c_time_last_us = 0U;
static volatile uint32_t s_c_time_max_us = 0U;
static volatile uint32_t s_c_time_over_budget = 0U;
static volatile uint32_t s_c_first_over_start_idx = 0U;
static volatile uint32_t s_c_first_over_us = 0U;
static volatile uint8_t  s_bc_first_over_stage = 0U; /* 0:none 1:B 2:C */

static UscPreprocIncremental_t s_preinc;

typedef struct
{
  uint8_t wake_detected;
  int32_t top1;
  int32_t top2;
  uint32_t score_hotword;
  uint32_t score_other;
  uint32_t score_top1;
  uint32_t score_top2;
} NeaiGateResult_t;

static float s_neai_input_signal[NEAI_INPUT_COUNT];
static float s_neai_probabilities[NEAI_NUMBER_OF_CLASSES];
static volatile uint32_t s_neai_ok = 0U;
static volatile uint32_t s_neai_err = 0U;
static volatile uint32_t s_neai_top1 = 0U;
static volatile uint32_t s_neai_top2 = 0U;
static volatile uint32_t s_neai_top1_p = 0U;
static volatile uint32_t s_neai_top2_p = 0U;
static volatile uint8_t s_neai_gate_armed = 0U;
static volatile uint8_t s_usc_mode_latched = 0U;
static volatile uint8_t s_neai_hotword_hits = 0U;
static volatile uint32_t s_usc_awake_windows_left = 0U;
static volatile uint8_t s_usc_pre_initialized = 0U;
static volatile uint8_t s_usc_ai_initialized = 0U;
static volatile uint8_t s_usc_pre_rearm_needed = 1U;
static uint32_t s_neai_next_start = 0U;
static uint8_t s_neai_dense_mode = 0U;
static int32_t s_neai_hotword_class = -1;
static int32_t s_neai_other_class = -1;
static uint8_t s_neai_initialized = 0U;
static volatile uint32_t s_neai_jump_count = 0U;
static volatile uint32_t s_neai_clamp_count = 0U;
static volatile uint32_t s_neai_view_fail_count = 0U;

static uint32_t s_t_audio_start_ms = 0U;
static uint8_t s_t_first_dma_logged = 0U;
static uint8_t s_t_first_neai_ready_logged = 0U;
static uint8_t s_t_first_neai_start_logged = 0U;
static uint8_t s_t_first_neai_done_logged = 0U;
static uint8_t s_t_first_usc_done_logged = 0U;

typedef struct
{
  uint8_t active;
  uint8_t cls;
  uint8_t hits;
  uint8_t frames_left;
  uint8_t suppress;
  uint8_t rearm_block;
  uint8_t quiet_hits;
  uint8_t accept_cooldown;
  uint32_t best_score;
  uint32_t best_margin;
} UscJudgeHold_t;

static UscJudgeHold_t s_usc_hold = {0U, USC_INVALID_CLASS, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};

#if (APP_RUN_MODE == APP_RUN_MODE_DATA_CAPTURE)
#define CAPTURE_SAMPLES_PER_TS   (1000U)
#define CAPTURE_UART_TIMEOUT_MS  (100U)
#define CAPTURE_CMD_BUF_LEN      (16U)
#define CAPTURE_RX_CHUNK_LEN     (32U)
#define CAPTURE_CMD_START        "START"
#define CAPTURE_CMD_STOP         "STOP"
#define CAPTURE_CMD_PING         "PING"
#define CAPTURE_ACK_READY        "READY\r\n"
#define CAPTURE_ACK_OK_START     "OK_START\r\n"
#define CAPTURE_ACK_OK_STOP      "OK_STOP\r\n"
#define CAPTURE_ACK_ERR_START    "ERR_START\r\n"

typedef struct
{
  uint8_t initialized;
  uint32_t n_samples_to_timestamp;
  uint64_t total_samples;
  double start_timestamp;
  double old_time_stamp;
  double ioffset;
} CaptureContext_t;

static volatile uint32_t g_uart1_err_code = 0U;
static volatile uint32_t g_uart1_err_count = 0U;
static volatile uint16_t g_uart1_last_rx_size = 0U;
static CaptureContext_t s_cap = {0};
static uint8_t s_stream_chunk_phase = 0U;
static uint8_t s_session_active = 0U;
static uint8_t s_cmd_buf[CAPTURE_CMD_BUF_LEN] = {0U};
static uint32_t s_cmd_len = 0U;
static volatile uint8_t s_cmd_ready = 0U;
static uint8_t s_cmd_rx_buf[CAPTURE_RX_CHUNK_LEN] = {0U};

static void Capture_CommandParserPushByte(uint8_t ch);
static void Capture_CommandParserPushBuffer(const uint8_t *buf, uint16_t len);
static void Capture_UartStartRxIT(void);
static void Capture_ResetSession(void);
static HAL_StatusTypeDef Capture_StartSession(void);
static void Capture_StopSession(void);
static HAL_StatusTypeDef Capture_StreamSamplesWithTimestamps(const int16_t *samples, uint32_t sample_count);
static void Capture_PollCommands(void);

static void Capture_ResetSession(void)
{
  (void)memset(&s_cap, 0, sizeof(s_cap));
  s_stream_chunk_phase = 0U;
}

static double Capture_GetNowTimestamp(void)
{
  return (double)PerfCounter_GetCycles() / (double)SystemCoreClock;
}

static HAL_StatusTypeDef Capture_StreamBytes(const uint8_t *buf, uint32_t size)
{
  if ((buf == NULL) || (size == 0U))
  {
    return HAL_OK;
  }
  return HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)size, CAPTURE_UART_TIMEOUT_MS);
}

static HAL_StatusTypeDef Capture_SendText(const char *s)
{
  return Capture_StreamBytes((const uint8_t *)s, (uint32_t)((s != NULL) ? strlen(s) : 0U));
}

static void Capture_ResetCommandParser(void)
{
  s_cmd_len = 0U;
  s_cmd_ready = 0U;
  (void)memset(s_cmd_buf, 0, sizeof(s_cmd_buf));
}

static void Capture_CommandParserPushByte(uint8_t ch)
{
  if (s_cmd_ready != 0U)
  {
    return;
  }

  if ((ch == (uint8_t)'\r') || (ch == (uint8_t)'\n'))
  {
    if (s_cmd_len > 0U)
    {
      s_cmd_buf[s_cmd_len] = 0U;
      s_cmd_ready = 1U;
    }
    return;
  }

  if (s_cmd_len < (CAPTURE_CMD_BUF_LEN - 1U))
  {
    if ((ch >= (uint8_t)'a') && (ch <= (uint8_t)'z'))
    {
      ch = (uint8_t)(ch - ((uint8_t)'a' - (uint8_t)'A'));
    }
    s_cmd_buf[s_cmd_len++] = ch;
  }
  else
  {
    Capture_ResetCommandParser();
  }
}

static void Capture_CommandParserPushBuffer(const uint8_t *buf, uint16_t len)
{
  uint16_t i;

  if ((buf == NULL) || (len == 0U))
  {
    return;
  }

  for (i = 0U; i < len; i++)
  {
    Capture_CommandParserPushByte(buf[i]);
    if (s_cmd_ready != 0U)
    {
      break;
    }
  }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart == &huart1)
  {
    g_uart1_last_rx_size = Size;
    Capture_CommandParserPushBuffer(s_cmd_rx_buf, Size);
    (void)HAL_UARTEx_ReceiveToIdle_IT(&huart1, s_cmd_rx_buf, sizeof(s_cmd_rx_buf));
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart1)
  {
    g_uart1_err_code = huart->ErrorCode;
    g_uart1_err_count++;

    __HAL_UART_CLEAR_PEFLAG(&huart1);
    __HAL_UART_CLEAR_FEFLAG(&huart1);
    __HAL_UART_CLEAR_NEFLAG(&huart1);
    __HAL_UART_CLEAR_OREFLAG(&huart1);

    (void)HAL_UARTEx_ReceiveToIdle_IT(&huart1, s_cmd_rx_buf, sizeof(s_cmd_rx_buf));
  }
}

static void Capture_UartStartRxIT(void)
{
  Capture_ResetCommandParser();
  (void)HAL_UARTEx_ReceiveToIdle_IT(&huart1, s_cmd_rx_buf, sizeof(s_cmd_rx_buf));
}

static void Capture_StopSession(void)
{
  if (AudioAcq_IsStarted() != 0U)
  {
    AudioAcq_Stop();
  }
  s_session_active = 0U;
  Capture_ResetSession();
  Capture_ResetCommandParser();
}

static HAL_StatusTypeDef Capture_StartSession(void)
{
  Capture_StopSession();
  AudioRingBuf_Init();
  s_cap.start_timestamp = Capture_GetNowTimestamp();
  if (AudioAcq_Start() != HAL_OK)
  {
    return HAL_ERROR;
  }
  s_session_active = 1U;
  return HAL_OK;
}

static void Capture_HandleCommandLine(const char *cmd)
{
  if ((cmd == NULL) || (cmd[0] == '\0'))
  {
    return;
  }

  if (strcmp(cmd, CAPTURE_CMD_PING) == 0)
  {
    (void)Capture_SendText(CAPTURE_ACK_READY);
  }
  else if (strcmp(cmd, CAPTURE_CMD_START) == 0)
  {
    if (Capture_StartSession() == HAL_OK)
    {
      (void)Capture_SendText(CAPTURE_ACK_OK_START);
    }
    else
    {
      (void)Capture_SendText(CAPTURE_ACK_ERR_START);
    }
  }
  else if (strcmp(cmd, CAPTURE_CMD_STOP) == 0)
  {
    Capture_StopSession();
    (void)Capture_SendText(CAPTURE_ACK_OK_STOP);
    (void)Capture_SendText(CAPTURE_ACK_READY);
  }
}

static void Capture_PollCommands(void)
{
  if (s_cmd_ready != 0U)
  {
    Capture_HandleCommandLine((const char *)s_cmd_buf);
    Capture_ResetCommandParser();
  }
}

static HAL_StatusTypeDef Capture_StreamSamplesWithTimestamps(const int16_t *samples, uint32_t sample_count)
{
  uint8_t *data_buf = (uint8_t *)samples;
  uint32_t samplesToSend = sample_count;
  const uint32_t nBytesPerSample = sizeof(int16_t);
  const double measuredODR = (double)AUDIO_SAMPLE_RATE_HZ;
  double p_evt_timestamp;
  HAL_StatusTypeDef st = HAL_OK;

  if ((samples == NULL) || (sample_count == 0U))
  {
    return HAL_OK;
  }

  p_evt_timestamp = s_cap.start_timestamp +
                    ((double)(s_cap.total_samples + sample_count - 1U) / measuredODR);

  if (s_cap.initialized == 0U)
  {
    s_cap.ioffset = p_evt_timestamp - ((1.0 / measuredODR) * (samplesToSend - 1U));
    s_cap.old_time_stamp = p_evt_timestamp;
    s_cap.n_samples_to_timestamp = CAPTURE_SAMPLES_PER_TS;
    s_cap.initialized = 1U;
  }

  while (samplesToSend > 0U)
  {
    if ((s_cap.n_samples_to_timestamp == 0U) ||
        (samplesToSend < s_cap.n_samples_to_timestamp))
    {
      st = Capture_StreamBytes(data_buf, samplesToSend * nBytesPerSample);
      if (st != HAL_OK)
      {
        return st;
      }

      if (s_cap.n_samples_to_timestamp != 0U)
      {
        s_cap.n_samples_to_timestamp -= samplesToSend;
      }
      samplesToSend = 0U;
    }
    else
    {
      double newTS;

      st = Capture_StreamBytes(data_buf, s_cap.n_samples_to_timestamp * nBytesPerSample);
      if (st != HAL_OK)
      {
        return st;
      }

      data_buf += s_cap.n_samples_to_timestamp * nBytesPerSample;
      samplesToSend -= s_cap.n_samples_to_timestamp;

      if (measuredODR != 0.0)
      {
        newTS = p_evt_timestamp - ((1.0 / measuredODR) * samplesToSend);
      }
      else
      {
        newTS = p_evt_timestamp;
      }

      st = Capture_StreamBytes((const uint8_t *)&newTS, 8U);
      if (st != HAL_OK)
      {
        return st;
      }

      s_cap.n_samples_to_timestamp = CAPTURE_SAMPLES_PER_TS;
    }
  }

  s_cap.total_samples += sample_count;
  s_cap.old_time_stamp = p_evt_timestamp;
  return HAL_OK;
}
#endif /* APP_RUN_MODE == APP_RUN_MODE_DATA_CAPTURE */

static VOID PreprocThread_Entry(ULONG thread_input);
static VOID AiThread_Entry(ULONG thread_input);
static VOID AtThread_Entry(ULONG thread_input);
static VOID App_Delay(ULONG Delay);
void SystemClock_Restore(void);
void Enter_LowPower_Mode(void);
void Exit_LowPower_Mode(void);
extern void App_SystemClock_SetLow(void);
extern void App_SystemClock_SetHigh(void);
extern uint32_t App_SystemClock_GetCurrentMHz(void);
static void UscAi_UpdateTop2(const float *scores, uint32_t count);
static const char *UscAi_ClassName(uint32_t cls);
static void UscAi_PrintTop2(void);
static uint8_t UscJudge_NormalizeClass(uint32_t raw_cls);
static uint8_t UscJudge_IsCommandClass(uint8_t cls, uint32_t score);
static uint8_t UscJudge_IsQuietFrame(uint8_t cls, uint32_t score);
static void UscJudge_ResetCandidate(void);
static void UscJudge_StartCandidate(uint8_t cls, uint32_t score, uint32_t margin);
static void UscJudge_UpdateCandidate(uint32_t score, uint32_t margin);
static uint8_t UscJudge_ShouldAcceptCandidate(void);
static void UscJudge_ProcessTop2(void);
static uint8_t UscMode_IsActive(void);
static const char *UscPreproc_ModeName(UscPreprocInc_Mode_t mode);
static const char *BcFirstOver_StageName(uint8_t stage);

static uint8_t Neai_StrEqIgnoreCase(const char *a, const char *b);
static uint8_t Neai_IsOtherClassName(const char *name);
static int32_t Neai_FindClassByName(const char *target);
static int32_t Neai_FindFirstWakeClass(void);
static const char *Neai_ClassName(int32_t cls);
static HAL_StatusTypeDef Neai_InitWrapper(void);
static HAL_StatusTypeDef Neai_RunView(const int16_t *seg1_ptr, uint32_t seg1_count,
                                      const int16_t *seg2_ptr, uint32_t seg2_count,
                                      NeaiGateResult_t *result);
static void Neai_PrintTop2(void);
static void NeaiGate_Wake(void);
static void NeaiGate_Sleep(const char *reason);
static void NeaiGate_ProcessResult(const NeaiGateResult_t *result);
static void NeaiGate_ProcessAvailableWindows(uint32_t total_written);
static void UscPipeline_HardClear(void);
static void UscAction_ReturnToNeaiMode(void);
#if (APP_HAS_HUART2 != 0)
static void AtUart_EnableHSI(void)
{
  __HAL_RCC_HSI_ENABLE();
  while (__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) == RESET)
  {
  }
}

static void MX_USART2_UART_Init(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  AtUart_EnableHSI();

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_HSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

static uint8_t AtUart_EnsureReady(void)
{
  if (s_at_uart_ready == 0U)
  {
    MX_USART2_UART_Init();
    s_at_uart_ready = 1U;
  }
  return s_at_uart_ready;
}
#endif
static const char *UscAction_GetAtCommand(uint8_t cls)
{
  switch (cls)
  {
    case 0U: return "AT+AMDATA_TMP=0000EB908866\r\n"; /* microwaveoff / legacy slot0 */
    case 1U: return "AT+AMDATA_TMP=0000EB908855\r\n"; /* microwaveon  / legacy slot1 */
    case 2U: return "AT+AMDATA_TMP=0000EB902266\r\n"; /* fanoff */
    case 3U: return "AT+AMDATA_TMP=0000EB902255\r\n"; /* fanon */
    case 4U: return "AT+AMDATA_TMP=0000EB906666\r\n"; /* humidifieroff */
    case 5U: return "AT+AMDATA_TMP=0000EB906655\r\n"; /* humidifieron */
    case 6U: return "AT+AMDATA_TMP=0000EB904466\r\n"; /* lightoff */
    case 7U: return "AT+AMDATA_TMP=0000EB904455\r\n"; /* lighton */
    case 8U: return "AT+AMDATA_TMP=0000EB900066\r\n"; /* Televisionoff */
    case 9U: return "AT+AMDATA_TMP=0000EB900055\r\n"; /* Televisionon */
    default: return NULL;
  }
}

static void UscAction_OnAccept(uint8_t cls, uint32_t score);
static uint8_t UscPreproc_EnsureReady(void);
static uint8_t UscAi_EnsureReady(void);

UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;
  CHAR *pointer = TX_NULL;
  ULONG slot;

  DebugUart_ThreadXSyncInit();

  if (tx_semaphore_create(&tx_audio_sem, "audio sem", 0U) != TX_SUCCESS)
  {
    return TX_SEMAPHORE_ERROR;
  }
  s_audio_sem_ready = 1U;

#if (APP_RUN_MODE == APP_RUN_MODE_RECOGNITION) && (APP_PROFILE_ENABLE_USC != 0)
  if (tx_semaphore_create(&tx_feat_free_sem, "feat free sem", FEAT_QUEUE_DEPTH) != TX_SUCCESS)
  {
    return TX_SEMAPHORE_ERROR;
  }

  if (tx_queue_create(&tx_raw_queue,
                      "raw queue",
                      TX_1_ULONG,
                      tx_raw_queue_storage,
                      sizeof(tx_raw_queue_storage)) != TX_SUCCESS)
  {
    return TX_QUEUE_ERROR;
  }

  if (tx_queue_create(&tx_feat_queue,
                      "feat queue",
                      TX_1_ULONG,
                      tx_feat_queue_storage,
                      sizeof(tx_feat_queue_storage)) != TX_SUCCESS)
  {
    return TX_QUEUE_ERROR;
  }

  if (tx_queue_create(&tx_free_feat_queue,
                      "free feat queue",
                      TX_1_ULONG,
                      tx_free_feat_queue_storage,
                      sizeof(tx_free_feat_queue_storage)) != TX_SUCCESS)
  {
    return TX_QUEUE_ERROR;
  }

  for (slot = 0U; slot < FEAT_QUEUE_DEPTH; ++slot)
  {
    if (tx_queue_send(&tx_free_feat_queue, &slot, TX_NO_WAIT) != TX_SUCCESS)
    {
      return TX_QUEUE_ERROR;
    }
  }

#if (APP_USC_SEND_AT_ON_ACCEPT != 0) && (APP_HAS_HUART2 != 0)
  if (tx_queue_create(&tx_at_queue,
                      "at queue",
                      TX_1_ULONG,
                      tx_at_queue_storage,
                      sizeof(tx_at_queue_storage)) != TX_SUCCESS)
  {
    return TX_QUEUE_ERROR;
  }
#endif
#endif

  if (tx_byte_allocate(byte_pool, (VOID**) &pointer,
                       TX_APP_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }
  if (tx_thread_create(&tx_app_thread, "AUDIO Producer", MainThread_Entry, 0, pointer,
                       TX_APP_STACK_SIZE, TX_APP_THREAD_PRIO, TX_APP_THREAD_PREEMPTION_THRESHOLD,
                       TX_APP_THREAD_TIME_SLICE, TX_APP_THREAD_AUTO_START) != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

#if (APP_RUN_MODE == APP_RUN_MODE_RECOGNITION) && (APP_PROFILE_ENABLE_USC != 0)
  pointer = TX_NULL;
  if (tx_byte_allocate(byte_pool, (VOID**) &pointer,
                       TX_PREPROC_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }
  if (tx_thread_create(&tx_preproc_thread, "USC Preproc", PreprocThread_Entry, 0, pointer,
                       TX_PREPROC_STACK_SIZE, TX_PREPROC_THREAD_PRIO, TX_PREPROC_THREAD_PRIO,
                       TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  pointer = TX_NULL;
  if (tx_byte_allocate(byte_pool, (VOID**) &pointer,
                       TX_AI_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }
  if (tx_thread_create(&tx_ai_thread, "USC AI", AiThread_Entry, 0, pointer,
                       TX_AI_STACK_SIZE, TX_AI_THREAD_PRIO, TX_AI_THREAD_PRIO,
                       TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

#if (APP_USC_SEND_AT_ON_ACCEPT != 0) && (APP_HAS_HUART2 != 0)
  pointer = TX_NULL;
  if (tx_byte_allocate(byte_pool, (VOID**) &pointer,
                       TX_AT_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }
  if (tx_thread_create(&tx_at_thread, "AT TX", AtThread_Entry, 0, pointer,
                       TX_AT_STACK_SIZE, TX_AT_THREAD_PRIO, TX_AT_THREAD_PRIO,
                       TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }
#endif
#endif

  return ret;
}

void MainThread_Entry(ULONG thread_input)
{
  (void)thread_input;
#if (APP_RUN_MODE == APP_RUN_MODE_DATA_CAPTURE)
  uint32_t committed_blocks;
  int16_t *p_dma;
  uint32_t i;

  TXLP5_LOG_INIT_PUTS("[THREADX-A] capture enter\r\n");
  PerfCounter_Init();
  AudioRingBuf_Init();
  AudioAcq_Init();
  Capture_StopSession();
  Capture_UartStartRxIT();
  (void)Capture_SendText(CAPTURE_ACK_READY);

  while (1)
  {
    (void)tx_semaphore_get(&tx_audio_sem, 1U);
    Capture_PollCommands();

    committed_blocks = AudioAcq_TakeCommittedBlockCount();
    p_dma = AudioAcq_GetDmaBuffer();
    for (i = 0U; i < committed_blocks; i++)
    {
      const int16_t *p_chunk;

      p_chunk = (s_stream_chunk_phase == 0U) ?
                &p_dma[0] :
                &p_dma[AUDIO_DMA_CHUNK_SAMPLES];

      s_stream_chunk_phase ^= 1U;

      if (s_session_active == 0U)
      {
        continue;
      }

      if (Capture_StreamSamplesWithTimestamps(p_chunk, AUDIO_DMA_CHUNK_SAMPLES) != HAL_OK)
      {
        Capture_StopSession();
        Capture_UartStartRxIT();
        (void)Capture_SendText(CAPTURE_ACK_READY);
        break;
      }
    }

  }
#endif
  uint32_t beat = 0U;
  uint32_t rate_x100 = 0U;
  uint32_t committed = 0U;
  uint32_t total_written = 0U;
  ULONG last_diag_tick = 0U;
  AudioAcq_Diag_t diag = {0};
  AudioWindow_Diag_t win_diag = {0};
  AudioWindow_Config_t win_cfg = {0};
  AudioWindow_View_t view = {0};
  UINT qret;

  TXLP5_LOG_INIT_PUTS("[THREADX-A] producer enter\r\n");
  PerfCounter_Init();
#if (APP_AUDIO_STAGE_SEL != APP_AUDIO_STAGE_RAW_DMA_ONLY)
  AudioRingBuf_Init();
#endif
  s_neai_next_start = 0U;
  s_neai_dense_mode = 0U;
  s_neai_gate_armed = 0U;
  s_usc_mode_latched = 0U;
  s_neai_hotword_hits = 0U;
  s_neai_dense_mode = 0U;
  s_usc_awake_windows_left = 0U;
  s_neai_jump_count = 0U;
  s_neai_clamp_count = 0U;
  s_neai_view_fail_count = 0U;
  (void)memset((void *)&s_usc_hold, 0, sizeof(s_usc_hold));
  s_usc_hold.cls = (uint8_t)USC_INVALID_CLASS;
  s_t_audio_start_ms = 0U;
  s_t_first_dma_logged = 0U;
  s_t_first_neai_ready_logged = 0U;
  s_t_first_neai_done_logged = 0U;
  s_t_first_usc_done_logged = 0U;
  (void)memset((void *)s_feat_slot_start_idx, 0, sizeof(s_feat_slot_start_idx));
  s_usc_infer_count = 0U;
  s_usc_jump_count = 0U;
  s_usc_last_start_idx = 0U;
  s_usc_last_tick = 0U;
  s_usc_last_dt_ms = 0U;
  s_usc_last_dstart = 0U;
  s_usc_pre_initialized = 0U;
  s_usc_ai_initialized = 0U;
  s_usc_pre_rearm_needed = 1U;
#if (APP_PROFILE_ENABLE_USC != 0)
  win_cfg.frame_samples = USC_FRAME_SAMPLES;
  win_cfg.hop_samples = USC_HOP_SAMPLES;
  AudioWindow_Init(&win_cfg);
  TXLP5_LOG_INIT_PRINT("[WIN-CFG] frame=%lu hop=%lu ring=%lu\r\n",
              (unsigned long)win_cfg.frame_samples,
              (unsigned long)win_cfg.hop_samples,
              (unsigned long)AudioRingBuf_GetCapacity());
  TXLP5_LOG_INIT_PRINT("[USC-CFG] in=%lu steps=%lu mfcc=%lu stride_cols=%lu\r\n",
              (unsigned long)USC_FEATURE_COUNT,
              (unsigned long)USC_COLS_TOTAL,
              (unsigned long)USC_COEFFS_PER_COL,
              (unsigned long)USC_COLS_PER_HOP);
#endif

#if (APP_NEAI_GATE_ENABLE != 0)
  if (Neai_InitWrapper() == HAL_OK)
  {
    App_TimingPulse_Event(APP_TIMING_EVENT_NEAI_INIT_DONE);
    TXLP5_LOG_TIMING_PRINT("[TTIM] neai-init-done t=%lu ms\r\n",
                (unsigned long)(HAL_GetTick() - g_app_boot_tick_ms));
    TXLP5_LOG_INIT_PRINT("[NEAI-CFG] in=%lu hotword=%ld name=%s\r\n",
                (unsigned long)NEAI_INPUT_COUNT,
                (long)s_neai_hotword_class,
                Neai_ClassName(s_neai_hotword_class));
  }
  else
  {
    TXLP5_LOG_INIT_PUTS("[NEAI] init failed\r\n");
  }
#else
  s_neai_gate_armed = 1U;
  s_usc_awake_windows_left = APP_USC_WAKE_MAX_INFER_FRAMES;
#endif

  AudioAcq_Init();
  if (AudioAcq_Start() != HAL_OK)
  {
    TXLP5_LOG_INIT_PUTS("[AUDIO] start failed\r\n");
    while (1)
    {
      HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
      tx_thread_sleep(50);
    }
  }
  s_t_audio_start_ms = HAL_GetTick();
  App_TimingPulse_Event(APP_TIMING_EVENT_AUDIO_START);
  TXLP5_LOG_TIMING_PRINT("[TTIM] audio-start t=%lu ms\r\n",
              (unsigned long)(s_t_audio_start_ms - g_app_boot_tick_ms));
  TXLP5_LOG_INIT_PUTS("[AUDIO] start ok\r\n");
  last_diag_tick = tx_time_get();

  while (1)
  {
    UINT sem_ret;
    uint32_t pending;

    sem_ret = tx_semaphore_get(&tx_audio_sem, AUDIO_EVENT_TIMEOUT_TICKS);
    (void)sem_ret;

    while (1)
    {
      pending = AudioAcq_TakeCommittedBlockCount();
      if (pending != 0U)
      {
        committed += pending;

        if (s_t_first_dma_logged == 0U)
        {
          s_t_first_dma_logged = 1U;
          App_TimingPulse_Event(APP_TIMING_EVENT_FIRST_DMA_BLOCK);
          TXLP5_LOG_TIMING_PRINT("[TTIM] first-dma-block t=%lu ms since-audio=%lu ms pending=%lu\r\n",
                      (unsigned long)(HAL_GetTick() - g_app_boot_tick_ms),
                      (unsigned long)(HAL_GetTick() - s_t_audio_start_ms),
                      (unsigned long)pending);
        }

#if (APP_AUDIO_STAGE_SEL == APP_AUDIO_STAGE_RAW_DMA_ONLY)
        /* A 模式只做采样对账，不做 ring/window/model 相关工作，尽量缩短唤醒驻留时间。 */
        continue;
#endif

#if (APP_PROFILE_ENABLE_NEAI != 0) || (APP_PROFILE_ENABLE_USC != 0)
        total_written = AudioRingBuf_GetTotalWritten();
#endif

#if (APP_PROFILE_ENABLE_NEAI != 0)
        NeaiGate_ProcessAvailableWindows(total_written);
#endif

#if (APP_PROFILE_ENABLE_USC != 0)
        if ((s_usc_mode_latched != 0U) && (s_neai_gate_armed == 0U))
        {
          s_neai_gate_armed = 1U;
        }

        /* Pre-wake stage: completely disable USC window maintenance/discard work.
         * We only start AudioWindow tracking after the first hotword wake,
         * where NeaiGate_Wake() performs a one-shot resync to the latest ready window. */
        if (UscMode_IsActive() != 0U)
        {
          AudioWindow_UpdateOnSamples(total_written);

          while (AudioWindow_HasReadyWindow() != 0U)
          {
            if (AudioWindow_PopNextWindowView(&view) != HAL_OK)
            {
              break;
            }

            {
              ULONG msg = (ULONG)view.start_index;
              qret = tx_queue_send(&tx_raw_queue, &msg, TX_WAIT_FOREVER);
              if (qret == TX_SUCCESS)
              {
                s_raw_sent++;
              }
              else
              {
                s_raw_drop++;
              }
            }
          }
        }
#endif
        continue;
      }

      if (tx_semaphore_get(&tx_audio_sem, TX_NO_WAIT) == TX_SUCCESS)
      {
        continue;
      }
      break;
    }

    if ((tx_time_get() - last_diag_tick) >= AUDIO_DIAG_PERIOD_TICKS)
    {
#if (APP_THREADX_HEARTBEAT_LOG_EN != 0)
      HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
      AudioAcq_GetDiag(&diag);
      rate_x100 = AudioAcq_GetMeasuredSampleRate_x100();
      AudioWindow_GetDiag(&win_diag);
      TXLP5_LOG_DIAG_PRINT("[AUDIO-CHK] beat=%lu blk=%lu smp=%lu q=%lu commit=%lu fs=%lu.%02lu ring=%lu pipe{raw=%lu/%u feat=%lu/%u dropR=%lu dropF=%lu doneC=%lu preOK=%lu preERR=%lu viewERR=%lu aiOK=%lu aiERR=%lu awake=%u nOK=%lu nERR=%lu}\r\n",
                  (unsigned long)beat,
                  (unsigned long)diag.dma_block_count,
                  (unsigned long)diag.dma_sample_count,
                  (unsigned long)diag.queue_depth,
                  (unsigned long)committed,
                  (unsigned long)(rate_x100 / 100U),
                  (unsigned long)(rate_x100 % 100U),
                  (unsigned long)AudioRingBuf_GetCount(),
                  (unsigned long)(s_raw_sent - s_raw_recv),
                  (unsigned)RAW_QUEUE_DEPTH,
                  (unsigned long)(s_feat_sent - s_feat_recv),
                  (unsigned)FEAT_QUEUE_DEPTH,
                  (unsigned long)s_raw_drop,
                  (unsigned long)s_feat_drop,
                  (unsigned long)s_c_done,
                  (unsigned long)s_pre_ok,
                  (unsigned long)s_pre_err,
                  (unsigned long)s_view_err,
                  (unsigned long)s_ai_ok,
                  (unsigned long)s_ai_err,
                  (unsigned)UscMode_IsActive(),
                  (unsigned long)s_neai_ok,
                  (unsigned long)s_neai_err);

      TXLP5_LOG_DIAG_PRINT("[PREM-CHK] beat=%lu boot=%lu rst=%lu fb=%lu inc=%lu cols=%lu\r\n",
                  (unsigned long)beat,
                  (unsigned long)s_pre_boot,
                  (unsigned long)s_pre_reset,
                  (unsigned long)s_pre_fallback,
                  (unsigned long)s_pre_inc,
                  (unsigned long)s_pre_inc_cols);

      TXLP5_LOG_DIAG_PRINT("[WIN-CHK] beat=%lu pop=%lu clamp=%lu skip=%lu backlog=%lu last{old=%lu nextB=%lu nextA=%lu pop=%lu}\r\n",
                  (unsigned long)beat,
                  (unsigned long)win_diag.pop_calls,
                  (unsigned long)win_diag.clamp_events,
                  (unsigned long)win_diag.clamped_hops,
                  (unsigned long)win_diag.ready_backlog,
                  (unsigned long)win_diag.last_oldest,
                  (unsigned long)win_diag.last_next_before,
                  (unsigned long)win_diag.last_next_after,
                  (unsigned long)win_diag.last_pop_start);

      TXLP5_LOG_DIAG_PRINT("[USC-CHK] beat=%lu infer=%lu jump=%lu last{dt=%lu ms dstart=%lu hop=%lu start=%lu} maxBacklog=%lu\r\n",
                  (unsigned long)beat,
                  (unsigned long)s_usc_infer_count,
                  (unsigned long)s_usc_jump_count,
                  (unsigned long)s_usc_last_dt_ms,
                  (unsigned long)s_usc_last_dstart,
                  (unsigned long)USC_HOP_SAMPLES,
                  (unsigned long)s_usc_last_start_idx,
                  (unsigned long)win_diag.max_ready_backlog);

      TXLP5_LOG_DIAG_PRINT("[BC-CHK] beat=%lu B{cnt=%lu last=%lu.%03lu max=%lu.%03lu over=%lu} C{cnt=%lu last=%lu.%03lu max=%lu.%03lu over=%lu} first=%s\r\n",
                  (unsigned long)beat,
                  (unsigned long)s_b_time_count,
                  (unsigned long)(s_b_time_last_us / 1000U),
                  (unsigned long)(s_b_time_last_us % 1000U),
                  (unsigned long)(s_b_time_max_us / 1000U),
                  (unsigned long)(s_b_time_max_us % 1000U),
                  (unsigned long)s_b_time_over_budget,
                  (unsigned long)s_c_time_count,
                  (unsigned long)(s_c_time_last_us / 1000U),
                  (unsigned long)(s_c_time_last_us % 1000U),
                  (unsigned long)(s_c_time_max_us / 1000U),
                  (unsigned long)(s_c_time_max_us % 1000U),
                  (unsigned long)s_c_time_over_budget,
                  BcFirstOver_StageName(s_bc_first_over_stage));
#else
      (void)diag;
      (void)rate_x100;
      (void)win_diag;
#endif

      beat++;
      last_diag_tick = tx_time_get();
    }
  }
}

static VOID PreprocThread_Entry(ULONG thread_input)
{
  (void)thread_input;
  ULONG raw_msg = 0U;
  ULONG feat_slot = 0U;
  AudioRingBuf_View_t ring_view = {0};
  AudioWindow_View_t pre_view = {0};
  UscPreprocInc_Mode_t pre_mode = USC_PREINC_MODE_FULL_BOOT;
  uint32_t new_cols = 0U;
  uint32_t b_start_cycles = 0U;
  uint32_t b_us = 0U;
  UINT qret;

  TXLP5_LOG_INIT_PUTS("[THREADX-B] preproc enter\r\n");

  while (1)
  {
    /* Acquire output capacity before consuming the next raw window.
     * This avoids popping an old raw start index and then waiting long enough
     * for the corresponding ring-buffer view to become stale/overwritten. */
    if (tx_semaphore_get(&tx_feat_free_sem, TX_WAIT_FOREVER) != TX_SUCCESS)
    {
      s_pre_err++;
      continue;
    }
    if (tx_queue_receive(&tx_free_feat_queue, &feat_slot, TX_WAIT_FOREVER) != TX_SUCCESS)
    {
      tx_semaphore_put(&tx_feat_free_sem);
      s_pre_err++;
      continue;
    }

    if (tx_queue_receive(&tx_raw_queue, &raw_msg, TX_WAIT_FOREVER) != TX_SUCCESS)
    {
      (void)tx_queue_send(&tx_free_feat_queue, &feat_slot, TX_NO_WAIT);
      (void)tx_semaphore_put(&tx_feat_free_sem);
      continue;
    }
    s_raw_recv++;

    if (UscPreproc_EnsureReady() == 0U)
    {
      (void)tx_queue_send(&tx_free_feat_queue, &feat_slot, TX_NO_WAIT);
      (void)tx_semaphore_put(&tx_feat_free_sem);
      s_pre_err++;
      continue;
    }

    if (AudioRingBuf_GetViewByAbsoluteIndex((uint32_t)raw_msg,
                                            USC_FRAME_SAMPLES,
                                            &ring_view) != HAL_OK)
    {
      (void)tx_queue_send(&tx_free_feat_queue, &feat_slot, TX_NO_WAIT);
      (void)tx_semaphore_put(&tx_feat_free_sem);
      s_view_err++;
      s_pre_err++;
      continue;
    }

    pre_view.start_index = (uint32_t)raw_msg;
    if (feat_slot < FEAT_QUEUE_DEPTH)
    {
      s_feat_slot_start_idx[feat_slot] = (uint32_t)raw_msg;
    }
    pre_view.sample_count = USC_FRAME_SAMPLES;
    pre_view.seg1_ptr = ring_view.seg1_ptr;
    pre_view.seg1_count = ring_view.seg1_count;
    pre_view.seg2_ptr = ring_view.seg2_ptr;
    pre_view.seg2_count = ring_view.seg2_count;

    b_start_cycles = PerfCounter_GetCycles();
    if (UscPreprocIncremental_ProcessView(&s_preinc,
                                          &pre_view,
                                          s_feat_slots[feat_slot],
                                          &pre_mode,
                                          &new_cols) == USC_PREINC_OK)
    {
      b_us = PerfCounter_CyclesToUs(PerfCounter_GetCycles() - b_start_cycles);
      s_b_time_count++;
      s_b_time_last_us = b_us;
      if (b_us > s_b_time_max_us)
      {
        s_b_time_max_us = b_us;
      }
      if (b_us > BC_HOP_BUDGET_US)
      {
        s_b_time_over_budget++;
        if (s_b_first_over_us == 0U)
        {
          s_b_first_over_us = b_us;
          s_b_first_over_start_idx = (uint32_t)raw_msg;
        }
        if (s_bc_first_over_stage == 0U)
        {
          s_bc_first_over_stage = 1U;
          TXLP5_LOG_DIAG_PRINT("[BC-FIRST] stage=B hop=%lu start=%lu mode=%s cols=%lu t=%lu.%03lu ms core=%luMHz\r\n",
                      (unsigned long)s_b_time_count,
                      (unsigned long)raw_msg,
                      UscPreproc_ModeName(pre_mode),
                      (unsigned long)new_cols,
                      (unsigned long)(b_us / 1000U),
                      (unsigned long)(b_us % 1000U),
                      (unsigned long)App_SystemClock_GetCurrentMHz());
        }
      }
      switch (pre_mode)
      {
        case USC_PREINC_MODE_FULL_BOOT:       s_pre_boot++; break;
        case USC_PREINC_MODE_FULL_RESET:      s_pre_reset++; break;
        case USC_PREINC_MODE_CONTIG_FALLBACK: s_pre_fallback++; break;
        case USC_PREINC_MODE_CONTIG_INC:      s_pre_inc++; break;
        default: break;
      }
      s_pre_inc_cols += new_cols;

      qret = tx_queue_send(&tx_feat_queue, &feat_slot, TX_WAIT_FOREVER);
      if (qret == TX_SUCCESS)
      {
        s_feat_sent++;
        s_pre_ok++;
      }
      else
      {
        /* Give slot back on queue send failure. */
        (void)tx_queue_send(&tx_free_feat_queue, &feat_slot, TX_NO_WAIT);
        (void)tx_semaphore_put(&tx_feat_free_sem);
        s_feat_drop++;
        s_pre_err++;
      }
    }
    else
    {
      b_us = PerfCounter_CyclesToUs(PerfCounter_GetCycles() - b_start_cycles);
      s_b_time_count++;
      s_b_time_last_us = b_us;
      if (b_us > s_b_time_max_us)
      {
        s_b_time_max_us = b_us;
      }
      if (b_us > BC_HOP_BUDGET_US)
      {
        s_b_time_over_budget++;
        if (s_b_first_over_us == 0U)
        {
          s_b_first_over_us = b_us;
          s_b_first_over_start_idx = (uint32_t)raw_msg;
        }
        if (s_bc_first_over_stage == 0U)
        {
          s_bc_first_over_stage = 1U;
          TXLP5_LOG_DIAG_PRINT("[BC-FIRST] stage=B hop=%lu start=%lu mode=ERR cols=%lu t=%lu.%03lu ms core=%luMHz\r\n",
                      (unsigned long)s_b_time_count,
                      (unsigned long)raw_msg,
                      (unsigned long)new_cols,
                      (unsigned long)(b_us / 1000U),
                      (unsigned long)(b_us % 1000U),
                      (unsigned long)App_SystemClock_GetCurrentMHz());
        }
      }
      (void)tx_queue_send(&tx_free_feat_queue, &feat_slot, TX_NO_WAIT);
      (void)tx_semaphore_put(&tx_feat_free_sem);
      s_pre_err++;
    }

  }
}

static VOID AiThread_Entry(ULONG thread_input)
{
  (void)thread_input;
  ULONG feat_msg = 0U;
  float out_scores[AI_USC_NETWORK_OUT_1_SIZE];
  uint32_t c_start_cycles = 0U;
  uint32_t c_us = 0U;

  TXLP5_LOG_INIT_PUTS("[THREADX-C] ai enter\r\n");

  while (1)
  {
    if (tx_queue_receive(&tx_feat_queue, &feat_msg, TX_WAIT_FOREVER) != TX_SUCCESS)
    {
      continue;
    }

#if (APP_NEAI_GATE_ENABLE != 0)
    if ((s_usc_mode_latched != 0U) && (s_neai_gate_armed == 0U))
    {
      s_neai_gate_armed = 1U;
    }

    if (UscMode_IsActive() == 0U)
    {
      s_feat_recv++;
      s_c_done++;
      (void)tx_queue_send(&tx_free_feat_queue, &feat_msg, TX_NO_WAIT);
      (void)tx_semaphore_put(&tx_feat_free_sem);
      continue;
    }
#endif

    if ((feat_msg < FEAT_QUEUE_DEPTH) && (UscAi_EnsureReady() != 0U))
    {
      c_start_cycles = PerfCounter_GetCycles();
      if (UscAiRunner_Run(s_feat_slots[feat_msg], USC_FEATURE_COUNT,
                          out_scores, AI_USC_NETWORK_OUT_1_SIZE) == HAL_OK)
      {
        c_us = PerfCounter_CyclesToUs(PerfCounter_GetCycles() - c_start_cycles);
        s_c_time_count++;
        s_c_time_last_us = c_us;
        if (c_us > s_c_time_max_us)
        {
          s_c_time_max_us = c_us;
        }
        if (s_t_first_usc_done_logged == 0U)
        {
          s_t_first_usc_done_logged = 1U;
          App_TimingPulse_Event(APP_TIMING_EVENT_FIRST_USC_DONE);
          TXLP5_LOG_TIMING_PRINT("[TTIM] first-usc-done t=%lu ms since-audio=%lu ms\r\n",
                      (unsigned long)(HAL_GetTick() - g_app_boot_tick_ms),
                      (unsigned long)(HAL_GetTick() - s_t_audio_start_ms));
        }
        {
          uint32_t start_idx = 0U;
          uint32_t now_ms = HAL_GetTick();
          uint32_t dt_ms = 0U;
          uint32_t dstart = 0U;

          if (feat_msg < FEAT_QUEUE_DEPTH)
          {
            start_idx = s_feat_slot_start_idx[feat_msg];
          }

          if (s_usc_infer_count != 0U)
          {
            dt_ms = now_ms - s_usc_last_tick;
            dstart = start_idx - s_usc_last_start_idx;
            if (dstart != USC_HOP_SAMPLES)
            {
              s_usc_jump_count++;
              TXLP5_LOG_JUMP_PRINT("[USC-JMP] infer=%lu prev=%lu curr=%lu dstart=%lu hop=%lu dt=%lu ms\r\n",
                          (unsigned long)(s_usc_infer_count + 1U),
                          (unsigned long)s_usc_last_start_idx,
                          (unsigned long)start_idx,
                          (unsigned long)dstart,
                          (unsigned long)USC_HOP_SAMPLES,
                          (unsigned long)dt_ms);
            }
          }

          s_usc_infer_count++;
          s_usc_last_start_idx = start_idx;
          s_usc_last_tick = now_ms;
          s_usc_last_dt_ms = dt_ms;
          s_usc_last_dstart = dstart;
        }

        if (c_us > BC_HOP_BUDGET_US)
        {
          s_c_time_over_budget++;
          if (s_c_first_over_us == 0U)
          {
            s_c_first_over_us = c_us;
            s_c_first_over_start_idx = (feat_msg < FEAT_QUEUE_DEPTH) ? s_feat_slot_start_idx[feat_msg] : 0U;
          }
          if (s_bc_first_over_stage == 0U)
          {
            s_bc_first_over_stage = 2U;
            TXLP5_LOG_DIAG_PRINT("[BC-FIRST] stage=C infer=%lu start=%lu t=%lu.%03lu ms dstart=%lu core=%luMHz\r\n",
                        (unsigned long)s_c_time_count,
                        (unsigned long)((feat_msg < FEAT_QUEUE_DEPTH) ? s_feat_slot_start_idx[feat_msg] : 0U),
                        (unsigned long)(c_us / 1000U),
                        (unsigned long)(c_us % 1000U),
                        (unsigned long)s_usc_last_dstart,
                        (unsigned long)App_SystemClock_GetCurrentMHz());
          }
        }

        UscAi_UpdateTop2(out_scores, AI_USC_NETWORK_OUT_1_SIZE);
        UscAi_PrintTop2();
        UscJudge_ProcessTop2();
        s_ai_ok++;
      }
      else
      {
        c_us = PerfCounter_CyclesToUs(PerfCounter_GetCycles() - c_start_cycles);
        s_c_time_count++;
        s_c_time_last_us = c_us;
        if (c_us > s_c_time_max_us)
        {
          s_c_time_max_us = c_us;
        }
        if (c_us > BC_HOP_BUDGET_US)
        {
          s_c_time_over_budget++;
          if (s_c_first_over_us == 0U)
          {
            s_c_first_over_us = c_us;
            s_c_first_over_start_idx = (feat_msg < FEAT_QUEUE_DEPTH) ? s_feat_slot_start_idx[feat_msg] : 0U;
          }
          if (s_bc_first_over_stage == 0U)
          {
            s_bc_first_over_stage = 2U;
            TXLP5_LOG_DIAG_PRINT("[BC-FIRST] stage=C infer=%lu start=%lu t=%lu.%03lu ms dstart=ERR core=%luMHz\r\n",
                        (unsigned long)s_c_time_count,
                        (unsigned long)((feat_msg < FEAT_QUEUE_DEPTH) ? s_feat_slot_start_idx[feat_msg] : 0U),
                        (unsigned long)(c_us / 1000U),
                        (unsigned long)(c_us % 1000U),
                        (unsigned long)App_SystemClock_GetCurrentMHz());
          }
        }
        s_ai_err++;
      }
    }
    else
    {
      s_ai_err++;
    }

    s_feat_recv++;
    s_c_done++;

    /* Return slot ownership to B. */
    (void)tx_queue_send(&tx_free_feat_queue, &feat_msg, TX_NO_WAIT);
    (void)tx_semaphore_put(&tx_feat_free_sem);

  }
}

static void UscAi_UpdateTop2(const float *scores, uint32_t count)
{
  uint32_t i;
  uint32_t top1 = 0U;
  uint32_t top2 = 0U;

  if ((scores == NULL) || (count == 0U))
  {
    return;
  }

  for (i = 1U; i < count; i++)
  {
    if (scores[i] > scores[top1])
    {
      top2 = top1;
      top1 = i;
    }
    else if ((i != top1) && ((top2 == top1) || (scores[i] > scores[top2])))
    {
      top2 = i;
    }
  }

  s_ai_top1 = top1;
  s_ai_top2 = top2;
  s_ai_top1_p = (uint32_t)(scores[top1] * 1000.0f + 0.5f);
  s_ai_top2_p = (uint32_t)(scores[top2] * 1000.0f + 0.5f);
}

static const char *UscAi_ClassName(uint32_t cls)
{
  static const char * const s_cls_name[AI_USC_NETWORK_OUT_1_SIZE] =
  {
	"microwaveoff",
	"microwaveon",
	"fanoff",
	"fanon",
	"humidifieroff",
	"humidifieron",
	"lightoff",
	"lighton",
	"Televisionoff",
	"Televisionon",
	"background"
  };

  if (cls < AI_USC_NETWORK_OUT_1_SIZE)
  {
    return s_cls_name[cls];
  }
  return "unknown";
}

static void UscAi_PrintTop2(void)
{
  const char *t1 = UscAi_ClassName(s_ai_top1);
  const char *t2 = UscAi_ClassName(s_ai_top2);
  uint32_t p1_i = s_ai_top1_p / 1000U;
  uint32_t p1_f = (s_ai_top1_p % 1000U) / 10U;
  uint32_t p2_i = s_ai_top2_p / 1000U;
  uint32_t p2_f = (s_ai_top2_p % 1000U) / 10U;
  uint32_t score2 = (s_ai_top2_p + 5U) / 10U;

  if (s_ai_top1 == USC_BG_CLASS)
  {
    if ((s_ai_top2 != USC_BG_CLASS) &&
        (s_ai_top2 < AI_USC_NETWORK_OUT_1_SIZE) &&
        (score2 >= USC_BG_PREVIEW_THRESHOLD))
    {
      TXLP5_LOG_RECOG_PRINT("[UB] %s %lu.%02lu | %s %lu.%02lu\r\n",
                  t1,
                  (unsigned long)p1_i,
                  (unsigned long)p1_f,
                  t2,
                  (unsigned long)p2_i,
                  (unsigned long)p2_f);
    }
    return;
  }

  TXLP5_LOG_RECOG_PRINT("[U] %s %lu.%02lu | %s %lu.%02lu\r\n",
              t1,
              (unsigned long)p1_i,
              (unsigned long)p1_f,
              t2,
              (unsigned long)p2_i,
              (unsigned long)p2_f);
}

static uint8_t UscJudge_NormalizeClass(uint32_t raw_cls)
{
  if (raw_cls >= AI_USC_NETWORK_OUT_1_SIZE)
  {
    return (uint8_t)USC_INVALID_CLASS;
  }

  return (uint8_t)raw_cls;
}

static uint8_t UscJudge_IsCommandClass(uint8_t cls, uint32_t score)
{
  if ((cls == (uint8_t)USC_BG_CLASS) || (cls == (uint8_t)USC_INVALID_CLASS))
  {
    return 0U;
  }

  if (score < USC_ENTER_THRESHOLD)
  {
    return 0U;
  }

  return 1U;
}

static uint8_t UscJudge_IsQuietFrame(uint8_t cls, uint32_t score)
{
  if ((cls == (uint8_t)USC_BG_CLASS) || (cls == (uint8_t)USC_INVALID_CLASS))
  {
    return 1U;
  }

  if (score <= USC_REARM_SCORE_MAX)
  {
    return 1U;
  }

  return 0U;
}

static uint8_t UscPreproc_EnsureReady(void)
{
#if (APP_PROFILE_ENABLE_USC == 0)
  return 0U;
#else
  if (s_usc_pre_initialized == 0U)
  {
    if ((UscPreproc_Init() == HAL_OK) &&
        (UscPreproc_GetExpectedFrameSamples() == USC_FRAME_SAMPLES) &&
        (UscPreproc_GetExpectedFeatureCount() == USC_FEATURE_COUNT) &&
        (UscPreproc_GetNumColumns() == USC_COLS_TOTAL) &&
        (UscPreproc_GetNumCoeffsPerColumn() == USC_COEFFS_PER_COL))
    {
      s_usc_pre_initialized = 1U;
      TXLP5_LOG_INIT_PUTS("[USC-PRE] init ok\r\n");
    }
    else
    {
      TXLP5_LOG_INIT_PUTS("[USC-PRE] init failed\r\n");
      return 0U;
    }
  }

  if (s_usc_pre_rearm_needed != 0U)
  {
    if (UscPreprocIncremental_Init(&s_preinc,
                                   USC_FRAME_SAMPLES,
                                   USC_HOP_SAMPLES,
                                   USC_COLS_TOTAL,
                                   USC_COEFFS_PER_COL,
                                   USC_COLS_PER_HOP) != USC_PREINC_OK)
    {
      TXLP5_LOG_INIT_PUTS("[USC-PRE] preinc init failed\r\n");
      return 0U;
    }
    s_usc_pre_rearm_needed = 0U;
  }

  return 1U;
#endif
}

static uint8_t UscAi_EnsureReady(void)
{
#if (APP_PROFILE_ENABLE_USC == 0)
  return 0U;
#else
  if (s_usc_ai_initialized == 0U)
  {
    if (UscAiRunner_Init() == HAL_OK)
    {
      s_usc_ai_initialized = 1U;
      TXLP5_LOG_USCAI_PUTS("[USC-AI] init ok\r\n");
    }
    else
    {
      TXLP5_LOG_USCAI_PUTS("[USC-AI] init failed\r\n");
      return 0U;
    }
  }

  return 1U;
#endif
}

static void UscJudge_ResetCandidate(void)
{
  s_usc_hold.active = 0U;
  s_usc_hold.cls = (uint8_t)USC_INVALID_CLASS;
  s_usc_hold.hits = 0U;
  s_usc_hold.frames_left = 0U;
  s_usc_hold.best_score = 0U;
  s_usc_hold.best_margin = 0U;
}

static void UscJudge_StartCandidate(uint8_t cls, uint32_t score, uint32_t margin)
{
  s_usc_hold.active = 1U;
  s_usc_hold.cls = cls;
  s_usc_hold.hits = 1U;
  s_usc_hold.frames_left = USC_HOLD_FRAMES;
  s_usc_hold.best_score = score;
  s_usc_hold.best_margin = margin;

  TXLP5_LOG_RECOG_PRINT("[UC] start %s s=%lu m=%lu left=%u\r\n",
              UscAi_ClassName(cls),
              (unsigned long)score,
              (unsigned long)margin,
              (unsigned)USC_HOLD_FRAMES);
}

static void UscJudge_UpdateCandidate(uint32_t score, uint32_t margin)
{
  s_usc_hold.hits++;

  if (score > s_usc_hold.best_score)
  {
    s_usc_hold.best_score = score;
  }

  if (margin > s_usc_hold.best_margin)
  {
    s_usc_hold.best_margin = margin;
  }

  TXLP5_LOG_RECOG_PRINT("[UC] update %s hits=%u best=%lu m=%lu left=%u\r\n",
              UscAi_ClassName(s_usc_hold.cls),
              (unsigned)s_usc_hold.hits,
              (unsigned long)s_usc_hold.best_score,
              (unsigned long)s_usc_hold.best_margin,
              (unsigned)s_usc_hold.frames_left);
}

static uint8_t UscJudge_ShouldAcceptCandidate(void)
{
  if (s_usc_hold.hits < USC_MIN_HITS)
  {
    return 0U;
  }

  if (s_usc_hold.best_score >= USC_ACCEPT_THRESHOLD)
  {
    return 1U;
  }

  if ((s_usc_hold.best_score >= USC_ACCEPT_HIT_SCORE) &&
      (s_usc_hold.best_margin >= USC_ACCEPT_MARGIN))
  {
    return 1U;
  }

  if ((s_usc_hold.best_margin >= (USC_ACCEPT_MARGIN + 4U)) &&
      (s_usc_hold.best_score >= USC_ACCEPT_MARGIN_SCORE))
  {
    return 1U;
  }

  return 0U;
}

static void UscJudge_ProcessTop2(void)
{
#if (APP_NEAI_GATE_ENABLE != 0)
  if (UscMode_IsActive() == 0U)
  {
    UscJudge_ResetCandidate();
    return;
  }
#endif
  uint8_t cls = UscJudge_NormalizeClass(s_ai_top1);
  uint8_t cls2 = UscJudge_NormalizeClass(s_ai_top2);
  uint32_t score = (s_ai_top1_p + 5U) / 10U;
  uint32_t score2 = (s_ai_top2_p + 5U) / 10U;
  uint32_t margin = (score > score2) ? (score - score2) : 0U;
  const char *name;

  (void)cls2;

  if (s_usc_hold.accept_cooldown > 0U)
  {
    s_usc_hold.accept_cooldown--;
    return;
  }


  if (s_usc_hold.suppress > 0U)
  {
    s_usc_hold.suppress--;
    return;
  }

  if (s_usc_hold.rearm_block != 0U)
  {
    if (UscJudge_IsQuietFrame(cls, score) != 0U)
    {
      if (s_usc_hold.quiet_hits < 0xFFU)
      {
        s_usc_hold.quiet_hits++;
      }

      if (s_usc_hold.quiet_hits >= USC_REARM_QUIET_FRAMES)
      {
        /* Re-arm command acceptance after 8 consecutive quiet/background-like frames. */
        s_usc_hold.rearm_block = 0U;
        s_usc_hold.quiet_hits = 0U;
      }
    }
    else
    {
      s_usc_hold.quiet_hits = 0U;
    }
    return;
  }

  if (s_usc_hold.active == 0U)
  {
    if (UscJudge_IsCommandClass(cls, score) != 0U)
    {
      UscJudge_StartCandidate(cls, score, margin);
    }
    return;
  }

  if ((UscJudge_IsCommandClass(cls, score) != 0U) && (cls == s_usc_hold.cls))
  {
    UscJudge_UpdateCandidate(score, margin);
  }
  else if ((UscJudge_IsCommandClass(cls, score) != 0U) &&
           (cls != s_usc_hold.cls) &&
           (score >= (s_usc_hold.best_score + USC_SWITCH_DELTA)))
  {
    TXLP5_LOG_RECOG_PRINT("[UC] switch %s->%s old=%lu new=%lu m=%lu\r\n",
                UscAi_ClassName(s_usc_hold.cls),
                UscAi_ClassName(cls),
                (unsigned long)s_usc_hold.best_score,
                (unsigned long)score,
                (unsigned long)margin);
    UscJudge_StartCandidate(cls, score, margin);
    return;
  }
  else
  {
    /* keep current candidate */
  }

  if (s_usc_hold.frames_left > 0U)
  {
    s_usc_hold.frames_left--;
  }

  if (s_usc_hold.frames_left == 0U)
  {
    name = UscAi_ClassName(s_usc_hold.cls);

    if (UscJudge_ShouldAcceptCandidate() != 0U)
    {
      TXLP5_LOG_RECOG_PRINT("[UA] accept %s best=%lu hits=%u margin=%lu\r\n",
                  name,
                  (unsigned long)s_usc_hold.best_score,
                  (unsigned)s_usc_hold.hits,
                  (unsigned long)s_usc_hold.best_margin);
      s_usc_hold.suppress = USC_SUPPRESS_FRAMES;
      s_usc_hold.rearm_block = 1U;
      s_usc_hold.quiet_hits = 0U;
      s_usc_hold.accept_cooldown = USC_ACCEPT_COOLDOWN_FRAMES;
      UscAction_OnAccept(s_usc_hold.cls, s_usc_hold.best_score);
    }
    else
    {
      TXLP5_LOG_RECOG_PRINT("[UA] reject %s best=%lu hits=%u margin=%lu\r\n",
                  name,
                  (unsigned long)s_usc_hold.best_score,
                  (unsigned)s_usc_hold.hits,
                  (unsigned long)s_usc_hold.best_margin);
    }

    UscJudge_ResetCandidate();
  }
}

static uint8_t Neai_StrEqIgnoreCase(const char *a, const char *b)
{
  unsigned char ca, cb;

  if ((a == NULL) || (b == NULL))
  {
    return 0U;
  }

  while ((*a != '\0') && (*b != '\0'))
  {
    ca = (unsigned char)(*a++);
    cb = (unsigned char)(*b++);
    if ((ca >= 'A') && (ca <= 'Z')) ca = (unsigned char)(ca - 'A' + 'a');
    if ((cb >= 'A') && (cb <= 'Z')) cb = (unsigned char)(cb - 'A' + 'a');
    if (ca != cb)
    {
      return 0U;
    }
  }

  return ((*a == '\0') && (*b == '\0')) ? 1U : 0U;
}

static uint8_t Neai_IsOtherClassName(const char *name)
{
  if (name == NULL)
  {
    return 0U;
  }

  if ((Neai_StrEqIgnoreCase(name, NEAI_OTHER_NAME_1) != 0U) ||
      (Neai_StrEqIgnoreCase(name, NEAI_OTHER_NAME_2) != 0U) ||
      (Neai_StrEqIgnoreCase(name, NEAI_OTHER_NAME_3) != 0U))
  {
    return 1U;
  }

  return 0U;
}

static int32_t Neai_FindClassByName(const char *target)
{
  int32_t i;

  for (i = 0; i < (int32_t)NEAI_NUMBER_OF_CLASSES; i++)
  {
    const char *name = neai_get_class_name((int)i);
    if (Neai_StrEqIgnoreCase(name, target) != 0U)
    {
      return i;
    }
  }

  return -1;
}

static int32_t Neai_FindFirstWakeClass(void)
{
  int32_t i;

  for (i = 0; i < (int32_t)NEAI_NUMBER_OF_CLASSES; i++)
  {
    const char *name = neai_get_class_name((int)i);
    if (Neai_IsOtherClassName(name) == 0U)
    {
      return i;
    }
  }

  return -1;
}

static const char *Neai_ClassName(int32_t cls)
{
  if ((cls >= 0) && (cls < (int32_t)NEAI_NUMBER_OF_CLASSES))
  {
    const char *name = neai_get_class_name((int)cls);
    return (name != NULL) ? name : "unknown";
  }

  return "unknown";
}

static HAL_StatusTypeDef Neai_InitWrapper(void)
{
  enum neai_state st;
  int32_t i;

  st = neai_classification_init();
  if (st != NEAI_OK)
  {
    s_neai_initialized = 0U;
    return HAL_ERROR;
  }

  s_neai_initialized = 1U;
  s_neai_hotword_class = Neai_FindClassByName(APP_NEAI_HOTWORD_NAME);
  if (s_neai_hotword_class < 0)
  {
    s_neai_hotword_class = Neai_FindFirstWakeClass();
  }

  s_neai_other_class = -1;
  for (i = 0; i < (int32_t)NEAI_NUMBER_OF_CLASSES; i++)
  {
    const char *name = neai_get_class_name((int)i);
    TXLP5_LOG_NEAI_PRINT("[NEAI] class %ld = %s\r\n", (long)i, (name != NULL) ? name : "(null)");
    if (Neai_IsOtherClassName(name) != 0U)
    {
      s_neai_other_class = i;
    }
  }

  return HAL_OK;
}

static HAL_StatusTypeDef Neai_RunView(const int16_t *seg1_ptr, uint32_t seg1_count,
                                      const int16_t *seg2_ptr, uint32_t seg2_count,
                                      NeaiGateResult_t *result)
{
  uint32_t i = 0U;
  uint32_t total = seg1_count + seg2_count;
  enum neai_state st;
  int id_class = 0;
  int32_t top1 = 0;
  int32_t top2 = 0;

  if ((result == NULL) || (s_neai_initialized == 0U) || (seg1_ptr == NULL) || (total != NEAI_INPUT_COUNT))
  {
    return HAL_ERROR;
  }

  for (i = 0U; i < seg1_count; i++)
  {
    s_neai_input_signal[i] = (float)seg1_ptr[i];
  }
  for (i = 0U; i < seg2_count; i++)
  {
    s_neai_input_signal[seg1_count + i] = (float)seg2_ptr[i];
  }

  (void)memset(result, 0, sizeof(*result));
  (void)memset(s_neai_probabilities, 0, sizeof(s_neai_probabilities));

#if (APP_SYSCLK_DYNAMIC_NEAI_BOOST != 0)
  App_SystemClock_SetHigh();
#endif
  st = neai_classification(s_neai_input_signal, s_neai_probabilities, &id_class);
#if (APP_SYSCLK_DYNAMIC_NEAI_BOOST != 0)
  App_SystemClock_SetLow();
#endif
  if (st != NEAI_OK)
  {
    return HAL_ERROR;
  }

  for (i = 1U; i < NEAI_NUMBER_OF_CLASSES; i++)
  {
    if (s_neai_probabilities[i] > s_neai_probabilities[top1])
    {
      top2 = top1;
      top1 = (int32_t)i;
    }
    else if (((int32_t)i != top1) && (((top2 == top1)) || (s_neai_probabilities[i] > s_neai_probabilities[top2])))
    {
      top2 = (int32_t)i;
    }
  }

  result->top1 = top1;
  result->top2 = top2;
  result->score_top1 = (uint32_t)(s_neai_probabilities[top1] * 1000.0f + 0.5f);
  result->score_top2 = (uint32_t)(s_neai_probabilities[top2] * 1000.0f + 0.5f);

  if ((s_neai_hotword_class >= 0) && (s_neai_hotword_class < (int32_t)NEAI_NUMBER_OF_CLASSES))
  {
    result->score_hotword = (uint32_t)(s_neai_probabilities[s_neai_hotword_class] * 1000.0f + 0.5f);
    result->wake_detected = (id_class == s_neai_hotword_class) ? 1U : 0U;
  }

  if ((s_neai_other_class >= 0) && (s_neai_other_class < (int32_t)NEAI_NUMBER_OF_CLASSES))
  {
    result->score_other = (uint32_t)(s_neai_probabilities[s_neai_other_class] * 1000.0f + 0.5f);
  }

  s_neai_top1 = (uint32_t)top1;
  s_neai_top2 = (uint32_t)top2;
  s_neai_top1_p = result->score_top1;
  s_neai_top2_p = result->score_top2;

  if (s_t_first_neai_done_logged == 0U)
  {
    s_t_first_neai_done_logged = 1U;
    App_TimingPulse_Event(APP_TIMING_EVENT_FIRST_NEAI_DONE);
    TXLP5_LOG_TIMING_PRINT("[TTIM] first-neai-done t=%lu ms since-audio=%lu ms top1=%s %lu.%02lu\r\n",
                (unsigned long)(HAL_GetTick() - g_app_boot_tick_ms),
                (unsigned long)(HAL_GetTick() - s_t_audio_start_ms),
                Neai_ClassName(top1),
                (unsigned long)(result->score_top1 / 1000U),
                (unsigned long)((result->score_top1 % 1000U) / 10U));
  }

  return HAL_OK;
}

static uint8_t UscMode_IsActive(void)
{
  return ((s_neai_gate_armed != 0U) || (s_usc_mode_latched != 0U)) ? 1U : 0U;
}

static const char *UscPreproc_ModeName(UscPreprocInc_Mode_t mode)
{
  switch (mode)
  {
    case USC_PREINC_MODE_FULL_BOOT:       return "FULL_BOOT";
    case USC_PREINC_MODE_FULL_RESET:      return "FULL_RESET";
    case USC_PREINC_MODE_CONTIG_FALLBACK: return "CONTIG_FALLBACK";
    case USC_PREINC_MODE_CONTIG_INC:      return "CONTIG_INC";
    default:                              return "UNKNOWN";
  }
}

static const char *BcFirstOver_StageName(uint8_t stage)
{
  switch (stage)
  {
    case 1U: return "B";
    case 2U: return "C";
    default: return "none";
  }
}

static void Neai_PrintTop2(void)
{
  const char *t1 = Neai_ClassName((int32_t)s_neai_top1);
  const char *t2 = Neai_ClassName((int32_t)s_neai_top2);
  uint32_t p1_i = s_neai_top1_p / 1000U;
  uint32_t p1_f = (s_neai_top1_p % 1000U) / 10U;
  uint32_t p2_i = s_neai_top2_p / 1000U;
  uint32_t p2_f = (s_neai_top2_p % 1000U) / 10U;

  TXLP5_LOG_NEAI_PRINT("[N] %s %lu.%02lu | %s %lu.%02lu awake=%u hits=%u\r\n",
              t1,
              (unsigned long)p1_i,
              (unsigned long)p1_f,
              t2,
              (unsigned long)p2_i,
              (unsigned long)p2_f,
              (unsigned)UscMode_IsActive(),
              (unsigned)s_neai_hotword_hits);
}

static void NeaiGate_Wake(void)
{
#if (APP_PROFILE_ENABLE_USC == 0)
  s_neai_hotword_hits = 0U;
  return;
#else
  if (s_usc_mode_latched == 0U)
  {
    TXLP5_LOG_RECOG_PRINT("[NW] wake %s\r\n", Neai_ClassName(s_neai_hotword_class));
    App_StageMark_Set(GPIO_PIN_SET);
  }

  /* First hotword permanently latches USC mode until power-off. */
  s_usc_mode_latched = 1U;
  s_neai_gate_armed = 1U;
#if (APP_SYSCLK_DYNAMIC_USC_BOOST != 0)
  App_SystemClock_SetHigh();
  TXLP5_LOG_INIT_PRINT("[CLK] USC latched core=%luMHz\r\n",
              (unsigned long)App_SystemClock_GetCurrentMHz());
#endif
  s_neai_hotword_hits = 0U;
  s_usc_awake_windows_left = 0U;
  UscJudge_ResetCandidate();
  s_usc_hold.suppress = 0U;
  s_usc_hold.rearm_block = 0U;
  s_usc_hold.quiet_hits = 0U;
  s_usc_hold.accept_cooldown = 0U;

  /* Pre-wake path no longer advances/discards AudioWindow state.
   * On the first wake, resync USC windows directly to the latest ready position
   * so USC starts from fresh audio without pre-wake bookkeeping load. */
  AudioWindow_Reset();
  AudioWindow_SeekToLatestReady(AudioRingBuf_GetTotalWritten());
#endif
}

static void NeaiGate_Sleep(const char *reason)
{
  /* Sleep/timeout path is intentionally disabled in the latched-USC build.
   * After the first wake, recognition stays in USC mode until power-off.
   * Also avoid hard-clearing queues here, so we do not proactively drop frames. */
  (void)reason;
}


static void NeaiGate_ProcessResult(const NeaiGateResult_t *result)
{
  if (result == NULL)
  {
    return;
  }

  if (result->score_hotword >= NEAI_DENSE_ENTER_SCORE)
  {
    s_neai_dense_mode = 1U;
  }
  else if (result->score_hotword < NEAI_DENSE_EXIT_SCORE)
  {
    s_neai_dense_mode = 0U;
  }

//  if ((result->wake_detected != 0U) &&
//      (result->score_hotword >= APP_NEAI_WAKE_SCORE_THRESHOLD) &&
//      (margin >= APP_NEAI_WAKE_MARGIN_THRESHOLD))
  if ((result->wake_detected != 0U) &&
      (result->score_hotword >= APP_NEAI_WAKE_SCORE_THRESHOLD))
  {
    if (s_neai_hotword_hits < 0xFFU)
    {
      s_neai_hotword_hits++;
    }

    if (s_neai_hotword_hits >= APP_NEAI_WAKE_HITS)
    {
      NeaiGate_Wake();
    }
  }
  else
  {
    s_neai_hotword_hits = 0U;
  }
}

static void NeaiGate_ProcessAvailableWindows(uint32_t total_written)
{
  AudioRingBuf_View_t ring_view;
  NeaiGateResult_t result;
  uint32_t processed = 0U;
  uint32_t total_now = total_written;
  uint32_t oldest_now = 0U;
  uint32_t prev_start = 0U;
  uint32_t next_step = 0U;

  if ((APP_PROFILE_ENABLE_NEAI == 0) || (s_neai_initialized == 0U) ||
      (s_neai_gate_armed != 0U) || (s_usc_mode_latched != 0U))
  {
    return;
  }

  while ((s_neai_gate_armed == 0U) &&
         (processed < NEAI_MAX_WINDOWS_PER_PASS))
  {
    total_now = AudioRingBuf_GetTotalWritten();
    oldest_now = AudioRingBuf_GetOldestAbsoluteIndex();

    if (s_neai_next_start < oldest_now)
    {
      prev_start = s_neai_next_start;
      s_neai_jump_count++;
      s_neai_clamp_count++;
      TXLP5_LOG_JUMP_PRINT("[NEAI-JMP] clamp prev=%lu oldest=%lu total=%lu dropped=%lu frame=%lu hop=%lu cnt=%lu\r\n",
                  (unsigned long)prev_start,
                  (unsigned long)oldest_now,
                  (unsigned long)total_now,
                  (unsigned long)(oldest_now - prev_start),
                  (unsigned long)AUDIO_NEAI_FRAME_SAMPLES,
                  (unsigned long)AUDIO_NEAI_HOP_SAMPLES,
                  (unsigned long)s_neai_clamp_count);

      if (total_now >= AUDIO_NEAI_FRAME_SAMPLES)
      {
        uint32_t latest_start = total_now - AUDIO_NEAI_FRAME_SAMPLES;
        s_neai_next_start = (latest_start > oldest_now) ? latest_start : oldest_now;
      }
      else
      {
        s_neai_next_start = oldest_now;
      }
    }

    if ((s_neai_next_start + AUDIO_NEAI_FRAME_SAMPLES) > total_now)
    {
      break;
    }

    if (s_t_first_neai_ready_logged == 0U)
    {
      s_t_first_neai_ready_logged = 1U;
      App_TimingPulse_Event(APP_TIMING_EVENT_FIRST_NEAI_WIN_READY);
      TXLP5_LOG_TIMING_PRINT("[TTIM] first-neai-window-ready t=%lu ms since-audio=%lu ms total=%lu oldest=%lu start=%lu\r\n",
                  (unsigned long)(HAL_GetTick() - g_app_boot_tick_ms),
                  (unsigned long)(HAL_GetTick() - s_t_audio_start_ms),
                  (unsigned long)total_now,
                  (unsigned long)oldest_now,
                  (unsigned long)s_neai_next_start);
    }

    if (AudioRingBuf_GetViewByAbsoluteIndex(s_neai_next_start,
                                            AUDIO_NEAI_FRAME_SAMPLES,
                                            &ring_view) != HAL_OK)
    {
      s_neai_err++;
      s_neai_jump_count++;
      s_neai_view_fail_count++;
      TXLP5_LOG_JUMP_PRINT("[NEAI-JMP] viewfail start=%lu oldest=%lu total=%lu frame=%lu hop=%lu cnt=%lu\r\n",
                  (unsigned long)s_neai_next_start,
                  (unsigned long)oldest_now,
                  (unsigned long)total_now,
                  (unsigned long)AUDIO_NEAI_FRAME_SAMPLES,
                  (unsigned long)AUDIO_NEAI_HOP_SAMPLES,
                  (unsigned long)s_neai_view_fail_count);

      total_now = AudioRingBuf_GetTotalWritten();
      oldest_now = AudioRingBuf_GetOldestAbsoluteIndex();
      if (total_now >= AUDIO_NEAI_FRAME_SAMPLES)
      {
        uint32_t latest_start = total_now - AUDIO_NEAI_FRAME_SAMPLES;
        s_neai_next_start = (latest_start > oldest_now) ? latest_start : oldest_now;
      }
      else
      {
        s_neai_next_start = oldest_now;
      }
      break;
    }

    if (s_t_first_neai_start_logged == 0U)
    {
      s_t_first_neai_start_logged = 1U;
      App_TimingPulse_Event(APP_TIMING_EVENT_FIRST_NEAI_START);
      TXLP5_LOG_TIMING_PRINT("[TTIM] first-neai-start t=%lu ms since-audio=%lu ms\r\n",
                  (unsigned long)(HAL_GetTick() - g_app_boot_tick_ms),
                  (unsigned long)(HAL_GetTick() - s_t_audio_start_ms));
    }

    if (Neai_RunView(ring_view.seg1_ptr, ring_view.seg1_count,
                     ring_view.seg2_ptr, ring_view.seg2_count,
                     &result) == HAL_OK)
    {
      s_neai_ok++;
      NeaiGate_ProcessResult(&result);
      if (s_neai_gate_armed == 0U)
      {
        Neai_PrintTop2();
      }
    }
    else
    {
      s_neai_err++;
    }

    processed++;

    if (s_neai_gate_armed != 0U)
    {
      break;
    }

    next_step = (s_neai_dense_mode != 0U) ?
                (AUDIO_NEAI_HOP_SAMPLES * NEAI_DENSE_HOP_MULTIPLIER) :
                (AUDIO_NEAI_HOP_SAMPLES * NEAI_QUIET_HOP_MULTIPLIER);
    if (next_step == 0U)
    {
      next_step = AUDIO_NEAI_HOP_SAMPLES;
    }
    s_neai_next_start += next_step;
  }
}

static void UscPipeline_HardClear(void)
{
  ULONG msg = 0U;
  AudioWindow_View_t view;

  while (tx_queue_receive(&tx_raw_queue, &msg, TX_NO_WAIT) == TX_SUCCESS)
  {
  }

  while (tx_queue_receive(&tx_feat_queue, &msg, TX_NO_WAIT) == TX_SUCCESS)
  {
    (void)tx_queue_send(&tx_free_feat_queue, &msg, TX_NO_WAIT);
    (void)tx_semaphore_put(&tx_feat_free_sem);
  }

  while (tx_semaphore_get(&tx_audio_sem, TX_NO_WAIT) == TX_SUCCESS)
  {
  }

  while (AudioWindow_HasReadyWindow() != 0U)
  {
    if (AudioWindow_PopNextWindowView(&view) != HAL_OK)
    {
      break;
    }
  }

  (void)memset(&s_preinc, 0, sizeof(s_preinc));
}

static void UscAction_ReturnToNeaiMode(void)
{
  uint32_t total_written = AudioRingBuf_GetTotalWritten();
  uint32_t oldest = AudioRingBuf_GetOldestAbsoluteIndex();

  s_neai_gate_armed = 0U;
  s_usc_mode_latched = 0U;
  s_neai_hotword_hits = 0U;
  s_usc_awake_windows_left = 0U;
  s_usc_pre_rearm_needed = 1U;
  UscJudge_ResetCandidate();
  s_usc_hold.suppress = 0U;
  s_usc_hold.rearm_block = 0U;
  s_usc_hold.quiet_hits = 0U;
  s_usc_hold.accept_cooldown = 0U;
  App_StageMark_Set(GPIO_PIN_RESET);
#if (APP_SYSCLK_DYNAMIC_USC_BOOST != 0)
  App_SystemClock_SetLow();
#endif

  UscPipeline_HardClear();
  AudioWindow_Reset();

  if (total_written >= AUDIO_NEAI_FRAME_SAMPLES)
  {
    s_neai_next_start = total_written - AUDIO_NEAI_FRAME_SAMPLES;
  }
  else
  {
    s_neai_next_start = 0U;
  }

  if (s_neai_next_start < oldest)
  {
    s_neai_next_start = oldest;
  }
}

static void UscAction_OnAccept(uint8_t cls, uint32_t score)
{
  (void)score;
#if (APP_USC_SEND_AT_ON_ACCEPT != 0) && (APP_HAS_HUART2 != 0)
  if (UscAction_GetAtCommand(cls) != NULL)
  {
    ULONG msg = (ULONG)cls;
    if (tx_queue_send(&tx_at_queue, &msg, TX_NO_WAIT) != TX_SUCCESS)
    {
      TXLP5_LOG_MISC_PRINT("[AT] queue full cls=%u\r\n", (unsigned)cls);
    }
  }
#endif
  /* First wake permanently latches USC mode until power-off.
   * Accept no longer forces a sleep/return-to-NEAI transition. */
}

static VOID AtThread_Entry(ULONG thread_input)
{
  (void)thread_input;
#if (APP_USC_SEND_AT_ON_ACCEPT != 0) && (APP_HAS_HUART2 != 0)
  ULONG at_msg = 0U;

  while (1)
  {
    if (tx_queue_receive(&tx_at_queue, &at_msg, TX_WAIT_FOREVER) != TX_SUCCESS)
    {
      continue;
    }

    {
      const char *p_cmd = UscAction_GetAtCommand((uint8_t)at_msg);
      if (p_cmd == NULL)
      {
        continue;
      }

      if (AtUart_EnsureReady() == 0U)
      {
        TXLP5_LOG_MISC_PRINT("[AT] uart2 init fail cls=%u\r\n", (unsigned)at_msg);
        continue;
      }

      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
      HAL_Delay(600U);//ok
//      HAL_Delay(500U);//no
      (void)HAL_UART_Transmit(&huart2, (uint8_t *)p_cmd, (uint16_t)strlen(p_cmd), 200U);
      HAL_Delay(15U);//ok
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

#if (APP_USC_RETURN_TO_NEAI_AFTER_AT != 0)
      UscAction_ReturnToNeaiMode();
#endif
    }
  }
#else
  while (1)
  {
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND);
  }
#endif
}

void AudioAcq_BlockCommittedFromISR(void)
{
  if (s_audio_sem_ready != 0U)
  {
    (void)tx_semaphore_put(&tx_audio_sem);
  }
}

void MX_ThreadX_Init(void)
{
  tx_kernel_enter();
}

void App_ThreadX_LowPower_Enter(void)
{
#if (APP_THREADX_IDLE_WFI_ENABLE != 0)
  HAL_SuspendTick();
  __DSB();
  __WFI();
  __ISB();
#endif
}

void App_ThreadX_LowPower_Exit(void)
{
#if (APP_THREADX_IDLE_WFI_ENABLE != 0)
  HAL_ResumeTick();
#endif
}

void SystemClock_Restore(void)
{
#if (APP_SYSCLK_BOOT_MHZ >= 160U)
  App_SystemClock_SetHigh();
#else
  App_SystemClock_SetLow();
#endif
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
  (void)GPIO_Pin;
}

static VOID App_Delay(ULONG Delay)
{
  tx_thread_sleep(Delay);
}
