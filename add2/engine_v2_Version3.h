#ifndef ENGINE_V2_H
#define ENGINE_V2_H
/*
 * engine_v2.h
 * Engine V2: minimal VM + SOC interface
 * - provides a tiny VM ticking each step and a SOC engine for dynamics
 */

#include "engine_nd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EngineV2 EngineV2;

/* create engine v2 (X,Y,P,VM registers) */
EngineV2 *engine_v2_create(int X, int Y, int P);
void engine_v2_free(EngineV2 *e);

/* run with vm_ticks per step */
int engine_v2_run(EngineV2 *e, int steps, int vm_ticks, float threshold, float dissipation, float phi_ethica, double inject_rate, float inject_strength, int verbose);

#ifdef __cplusplus
}
#endif

#endif // ENGINE_V2_H