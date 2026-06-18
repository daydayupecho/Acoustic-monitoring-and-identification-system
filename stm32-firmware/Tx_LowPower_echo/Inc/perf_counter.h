#ifndef PERF_COUNTER_H
#define PERF_COUNTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void PerfCounter_Init(void);
uint32_t PerfCounter_GetCycles(void);
uint32_t PerfCounter_CyclesToUs(uint32_t cycles);

#ifdef __cplusplus
}
#endif

#endif /* PERF_COUNTER_H */