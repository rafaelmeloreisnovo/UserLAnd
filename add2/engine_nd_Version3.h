#ifndef ENGINE_ND_H
#define ENGINE_ND_H

/*
 * engine_nd.h
 *
 * Generalized N-dimensional SOC engine (low-level C API)
 * - EngineND is the main "object" (struct) with opaque data pointer
 * - supports arbitrary number of dimensions (nd) with arbitrary per-dim sizes
 * - supports P pipelines per cell (channels)
 * - provides create/free, inject, step, run_loop, hashing, and metadata functions
 *
 * Build: see Makefile (requires OpenMP)
 */

#include <stdint.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EngineND EngineND;

/* Create engine:
 * nd: number of dimensions (>=2 typically)
 * dims: array of length nd with sizes for each dimension
 * P: number of pipelines per cell (channels)
 * returns NULL on failure
 */
EngineND *engine_create_nd(int nd, const int *dims, int P);

/* Free engine */
void engine_free_nd(EngineND *e);

/* Seed RNG for reproducible runs (call once) */
void engine_srand_nd(unsigned int seed);

/* Inject random energy:
 * inject_strength: base energy per injection
 * inject_rate: expected number of injections per step (absolute, not per cell)
 */
void engine_inject_nd(EngineND *e, float inject_strength, double inject_rate);

/* Perform one step:
 * - threshold, dissipation, phi_ethica
 * - out_n_topplings receives number of topplings in this step (if non-NULL)
 * returns delta_energy (negative if energy lost by dissipation)
 */
double engine_step_nd(EngineND *e, float threshold, float dissipation, float phi_ethica, unsigned long *out_n_topplings);

/* Run loop with watcher limits (convenience)
 * returns 0 on success, non-zero on watchdog abort or error
 */
int engine_run_loop_nd(EngineND *e,
                       int steps,
                       float threshold,
                       float dissipation,
                       float phi_ethica,
                       double inject_rate,
                       float inject_strength,
                       double watchdog_E_max,
                       unsigned long watchdog_K_max,
                       int verbose);

/* Compute 64-bit hash (FNV-1a) of the raw float data */
uint64_t engine_hash64_nd(EngineND *e);

/* Query functions */
size_t engine_cell_count_nd(const EngineND *e);
size_t engine_total_size_nd(const EngineND *e); /* total floats = prod(dims)*P */

/* Metadata / date */
const char *engine_build_date(void); /* compile-time date/time */
void engine_runtime_date(char *buf, size_t buflen); /* runtime ISO timestamp */

/* Debug: dump small region to stdout (for dev) */
void engine_dump_region_nd(const EngineND *e, int max_cells);

#ifdef __cplusplus
}
#endif

#endif // ENGINE_ND_H