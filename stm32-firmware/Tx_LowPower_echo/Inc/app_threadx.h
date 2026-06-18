#ifndef __APP_THREADX_H
#define __APP_THREADX_H
#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"
#include "main.h"

/* Queue depths */
#define RAW_QUEUE_DEPTH                     (32U)
#define FEAT_QUEUE_DEPTH                    (8U)
#define AT_QUEUE_DEPTH                      (4U)

/* Thread stack sizes */
#define TX_APP_STACK_SIZE                   (2048U)
#define TX_PREPROC_STACK_SIZE               (2048U)
#define TX_AI_STACK_SIZE                    (4096U)
#define TX_AT_STACK_SIZE                    (2048U)

/* Priorities: A highest, then B, then C */
#define TX_APP_THREAD_PRIO                  (8U)
#define TX_PREPROC_THREAD_PRIO              (9U)
#define TX_AI_THREAD_PRIO                   (10U)
#define TX_AT_THREAD_PRIO                   (11U)

#ifndef TX_APP_THREAD_PREEMPTION_THRESHOLD
#define TX_APP_THREAD_PREEMPTION_THRESHOLD  TX_APP_THREAD_PRIO
#endif
#ifndef TX_APP_THREAD_TIME_SLICE
#define TX_APP_THREAD_TIME_SLICE            TX_NO_TIME_SLICE
#endif
#ifndef TX_APP_THREAD_AUTO_START
#define TX_APP_THREAD_AUTO_START            TX_AUTO_START
#endif

UINT App_ThreadX_Init(VOID *memory_ptr);
void MX_ThreadX_Init(void);
void MainThread_Entry(ULONG thread_input);
void AudioAcq_BlockCommittedFromISR(void);

#ifdef __cplusplus
}
#endif
#endif /* __APP_THREADX_H */
