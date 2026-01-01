#ifndef ENGINE_V3_H
#define ENGINE_V3_H
/*
 * engine_v3.h
 * Engine V3: adds phi_ethica smoothing, refined micro-logs, and uses engine_nd
 */

#include "engine_nd.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EngineV3 EngineV3;

EngineV3 *engine_v3_create(int X, int Y, int P);
void engine_v3_free(EngineV3 *e);

int engine_v3_run(EngineV3 *e, int steps, float threshold, float dissipation, float phi_ethica, double inject_rate, float inject_strength, const char *out_json_path, int verbose);

#ifdef __cplusplus
}
#endif

#endif // ENGINE_V3_H