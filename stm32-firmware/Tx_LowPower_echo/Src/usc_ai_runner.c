#include "usc_ai_runner.h"

#include <string.h>

#include "debug_uart.h"
#include "power_test_cfg.h"
#include "usc_network.h"
#include "usc_network_data.h"

#define USC_AI_DIAG_PRINT_LIMIT   (0U)

#if (APP_USC_AI_LOG_EN != 0)
  #define USC_AI_LOG_PRINT(...)   Debug_Print(__VA_ARGS__)
#else
  #define USC_AI_LOG_PRINT(...)   do { } while (0)
#endif

static ai_handle s_network = AI_HANDLE_NULL;
static ai_buffer *s_inputs = NULL;
static ai_buffer *s_outputs = NULL;
AI_ALIGNED(32) static ai_u8 s_activations[AI_USC_NETWORK_DATA_ACTIVATIONS_SIZE];
AI_ALIGNED(32) static float s_input_stage[AI_USC_NETWORK_IN_1_SIZE];
AI_ALIGNED(32) static float s_output_stage[AI_USC_NETWORK_OUT_1_SIZE];
static float *s_network_input_ptr = NULL;
static float *s_network_output_ptr = NULL;
static uint8_t s_inited = 0U;
static uint32_t s_ai_diag_print_count = 0U;

static long UscAiRunner_ToMilli(float v)
{
  if (v >= 0.0f)
  {
    return (long)(v * 1000.0f + 0.5f);
  }
  return (long)(v * 1000.0f - 0.5f);
}

static uint32_t UscAiRunner_Checksum(const float *buf, uint32_t count)
{
  uint32_t i;
  uint32_t hash = 2166136261u;
  const uint8_t *p = (const uint8_t *)buf;
  uint32_t bytes = count * (uint32_t)sizeof(float);

  for (i = 0U; i < bytes; i++)
  {
    hash ^= p[i];
    hash *= 16777619u;
  }
  return hash;
}

static void UscAiRunner_PrintInputStats(const float *buf, uint32_t count)
{
  uint32_t i;
  float min_v;
  float max_v;
  double sum = 0.0;

  if ((buf == NULL) || (count == 0U))
  {
    return;
  }

  min_v = buf[0];
  max_v = buf[0];
  for (i = 0U; i < count; i++)
  {
    float v = buf[i];
    if (v < min_v) min_v = v;
    if (v > max_v) max_v = v;
    sum += (double)v;
  }

  USC_AI_LOG_PRINT("[USC-AI] in stats n=%lu min_m=%ld max_m=%ld mean_m=%ld crc=0x%08lx\r\n",
              (unsigned long)count,
              UscAiRunner_ToMilli(min_v),
              UscAiRunner_ToMilli(max_v),
              UscAiRunner_ToMilli((float)(sum / (double)count)),
              (unsigned long)UscAiRunner_Checksum(buf, count));
  USC_AI_LOG_PRINT("[USC-AI] in first12_m:");
  for (i = 0U; i < 12U && i < count; i++)
  {
    USC_AI_LOG_PRINT(" %ld", UscAiRunner_ToMilli(buf[i]));
  }
  USC_AI_LOG_PRINT("\r\n");
}

static void UscAiRunner_PrintOutputScores(const float *buf, uint32_t count)
{
  uint32_t i;
  USC_AI_LOG_PRINT("[USC-AI] out_m:");
  for (i = 0U; i < count; i++)
  {
    USC_AI_LOG_PRINT(" %lu=%ld", (unsigned long)i, UscAiRunner_ToMilli(buf[i]));
  }
  USC_AI_LOG_PRINT("\r\n");
}

static void UscAiRunner_EnsureCrcReady(void)
{
  __HAL_RCC_CRC_CLK_ENABLE();
  __HAL_RCC_CRC_FORCE_RESET();
  __HAL_RCC_CRC_RELEASE_RESET();

#ifdef CRC_CR_RESET
  CRC->CR = CRC_CR_RESET;
#endif

  USC_AI_LOG_PRINT("[CRC] clk=%lu\r\n",
              (unsigned long)(__HAL_RCC_CRC_IS_CLK_ENABLED() ? 1UL : 0UL));
  USC_AI_LOG_PRINT("[USC-AI] runner=txlp_crc_fix act=%lu net_in=%lu net_out=%lu\r\n",
              (unsigned long)AI_USC_NETWORK_DATA_ACTIVATIONS_SIZE,
              (unsigned long)AI_USC_NETWORK_IN_1_SIZE,
              (unsigned long)AI_USC_NETWORK_OUT_1_SIZE);
}

HAL_StatusTypeDef UscAiRunner_Init(void)
{
  ai_error err;
  const ai_handle acts[] = { s_activations };

  if (s_inited != 0U)
  {
    return HAL_OK;
  }

  UscAiRunner_EnsureCrcReady();

  err = ai_usc_network_create_and_init(&s_network, acts, NULL);
  if (err.type != AI_ERROR_NONE)
  {
    USC_AI_LOG_PRINT("[USC-AI] create_and_init failed: type=%d code=%d\r\n",
                (int)err.type, (int)err.code);
    s_network = AI_HANDLE_NULL;
    return HAL_ERROR;
  }

  s_inputs = ai_usc_network_inputs_get(s_network, NULL);
  s_outputs = ai_usc_network_outputs_get(s_network, NULL);
  if ((s_inputs == NULL) || (s_outputs == NULL))
  {
    USC_AI_LOG_PRINT("[USC-AI] io buffer query failed\r\n");
    (void)ai_usc_network_destroy(s_network);
    s_network = AI_HANDLE_NULL;
    return HAL_ERROR;
  }

  s_network_input_ptr = (float *)AI_HANDLE_PTR(s_inputs[0].data);
  s_network_output_ptr = (float *)AI_HANDLE_PTR(s_outputs[0].data);

  USC_AI_LOG_PRINT("[USC-AI] io staged in=%lu out=%lu\r\n",
              (unsigned long)(s_network_input_ptr != NULL ? 1UL : 0UL),
              (unsigned long)(s_network_output_ptr != NULL ? 1UL : 0UL));

  s_inited = 1U;
  return HAL_OK;
}

void UscAiRunner_DeInit(void)
{
  if (s_network != AI_HANDLE_NULL)
  {
    (void)ai_usc_network_destroy(s_network);
  }
  s_network = AI_HANDLE_NULL;
  s_inputs = NULL;
  s_outputs = NULL;
  s_network_input_ptr = NULL;
  s_network_output_ptr = NULL;
  s_inited = 0U;
}

HAL_StatusTypeDef UscAiRunner_Run(const float *in_features,
                                  uint32_t in_count,
                                  float *out_scores,
                                  uint32_t out_count)
{
  ai_i32 n_batch;

  if ((s_inited == 0U) || (s_network == AI_HANDLE_NULL) ||
      (s_inputs == NULL) || (s_outputs == NULL) ||
      (in_features == NULL) || (out_scores == NULL))
  {
    return HAL_ERROR;
  }

  if ((in_count != AI_USC_NETWORK_IN_1_SIZE) ||
      (out_count != AI_USC_NETWORK_OUT_1_SIZE))
  {
    USC_AI_LOG_PRINT("[USC-AI] size mismatch in=%lu/%lu out=%lu/%lu\r\n",
                (unsigned long)in_count,
                (unsigned long)AI_USC_NETWORK_IN_1_SIZE,
                (unsigned long)out_count,
                (unsigned long)AI_USC_NETWORK_OUT_1_SIZE);
    return HAL_ERROR;
  }

  if (s_ai_diag_print_count < USC_AI_DIAG_PRINT_LIMIT)
  {
    USC_AI_LOG_PRINT("[USC-AI] run diag #%lu\r\n", (unsigned long)(s_ai_diag_print_count + 1U));
    UscAiRunner_PrintInputStats(in_features, in_count);
  }

  memcpy(s_input_stage, in_features, in_count * sizeof(float));
  memset(s_output_stage, 0, out_count * sizeof(float));

  if (s_network_input_ptr != NULL)
  {
    memcpy(s_network_input_ptr, s_input_stage, in_count * sizeof(float));
    s_inputs[0].data = AI_HANDLE_PTR(s_network_input_ptr);
  }
  else
  {
    s_inputs[0].data = AI_HANDLE_PTR(s_input_stage);
  }

  if (s_network_output_ptr != NULL)
  {
    memset(s_network_output_ptr, 0, out_count * sizeof(float));
    s_outputs[0].data = AI_HANDLE_PTR(s_network_output_ptr);
  }
  else
  {
    s_outputs[0].data = AI_HANDLE_PTR(s_output_stage);
  }

  n_batch = ai_usc_network_run(s_network, s_inputs, s_outputs);
  if (n_batch != 1)
  {
    ai_error err = ai_usc_network_get_error(s_network);
    USC_AI_LOG_PRINT("[USC-AI] run failed: type=%d code=%d\r\n",
                (int)err.type, (int)err.code);
    return HAL_ERROR;
  }

  if (s_network_output_ptr != NULL)
  {
    memcpy(out_scores, s_network_output_ptr, out_count * sizeof(float));
  }
  else
  {
    memcpy(out_scores, s_output_stage, out_count * sizeof(float));
  }

  if (s_ai_diag_print_count < USC_AI_DIAG_PRINT_LIMIT)
  {
    UscAiRunner_PrintOutputScores(out_scores, out_count);
    s_ai_diag_print_count++;
  }

  return HAL_OK;
}
