#include "perf_counter.h"
#include "stm32u5xx.h"

void PerfCounter_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t PerfCounter_GetCycles(void)
{
  return DWT->CYCCNT;
}

uint32_t PerfCounter_CyclesToUs(uint32_t cycles)
{
  uint32_t clk = SystemCoreClock;
  if (clk == 0U)
  {
    return 0U;
  }

  return (uint32_t)(((uint64_t)cycles * 1000000ULL) / (uint64_t)clk);
}