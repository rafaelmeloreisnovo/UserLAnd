#ifndef ENGINE_ACCEL_H
#define ENGINE_ACCEL_H
/*
 * engine_accel.h
 * Engine Accelerated: orchestrates ND engine with options for cluster_mode (simulated),
 * profiling, logging, and Ethica hook interface (C).
 */

#include "engine_nd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EngineAccel EngineAccel;

/* create accel wrapper from existing ND dims */
EngineAccel *engine_accel_create(int nd, const int *dims, int P);
void engine_accel_free(EngineAccel *ea);

/* run with cluster_mode (simulate partitioning) */
int engine_accel_run(EngineAccel *ea, int steps, float threshold, float dissipation, float phi_ethica, double inject_rate, float inject_strength, int cluster_mode, int n_workers, const char *microlog_path, int verbose);

#ifdef __cplusplus
}
#endif

#endif // ENGINE_ACCEL_H