#ifndef ENGINE_V3_3D_H
#define ENGINE_V3_3D_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int X, Y, D, P;        // dims: spatial X,Y; D = dimensional depth; P = pipelines per cell
    float *data;          // flattened array: data[((x*Y + y)*D + d)*P + p]
    // meta
    unsigned long total_steps;
    double total_energy;
} EngineState;

/* create and free */
EngineState *engine_create(int X, int Y, int D, int P);
void engine_free(EngineState *s);

/* seed RNG (call once) */
void engine_srand(unsigned int seed);

/* inject random energy: rate = expected injections per cell per step */
void engine_inject(EngineState *s, float inject_strength, float inject_rate);

/* perform one soc step:
   threshold: threshold value
   dissipation: fraction of excess removed (0..1)
   phi_ethica: smoothing factor (0..1)
   out_n_topplings: pointer to int to receive number of topplings this step
   returns delta_energy (negative if energy lost) */
double engine_step(EngineState *s, float threshold, float dissipation, float phi_ethica, int *out_n_topplings);

/* compute simple 64-bit FNV hash of raw state (returns uint64) */
uint64_t engine_hash64(EngineState *s);

/* convenience run: steps loop with watchdogs */
int engine_run_loop(EngineState *s,
                    int steps,
                    float threshold,
                    float dissipation,
                    float phi_ethica,
                    float inject_rate,
                    float inject_strength,
                    double watchdog_E_max,
                    unsigned long watchdog_K_max);

#endif // ENGINE_V3_3D_H