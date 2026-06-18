#ifndef USC_PREPROC_TABLES_H
#define USC_PREPROC_TABLES_H

#include "arm_math.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const float32_t hannWin_512[512];
extern const uint32_t melFiltersStartIndices_512_32[32];
extern const uint32_t melFiltersStopIndices_512_32[32];
extern const float32_t melFilterLut_512_32[485];
extern const float32_t dct2_32_32[32*32];

#ifdef __cplusplus
}
#endif

#endif
