#ifndef ENGINE_MINIMAL_H
#define ENGINE_MINIMAL_H
/*
 * engine_minimal.h
 * 2D SOC minimal engine wrapper (C) using engine_nd API (nd=2)
 */

#include "engine_nd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EngineMinimal EngineMinimal;

/* create minimal engine (X,Y,P channels) */
EngineMinimal *engine_minimal_create(int X, int Y, int P);
void engine_minimal_free(EngineMinimal *m);

/* convenience run */
int engine_minimal_run(EngineMinimal *m, int steps, float threshold, float dissipation, float phi_ethica, double inject_rate, float inject_strength, int verbose);

#ifdef __cplusplus
}
#endif

#endif // ENGINE_MINIMAL_H