#include "engine_v3_3d.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <omp.h>

/* Flatten index macro */
#define IDX(s,x,y,d,p) ( (((size_t)(x) * (s)->Y + (y)) * (s)->D + (d)) * (s)->P + (p) )

/* RNG (simple) */
static unsigned int global_seed = 123456789u;
void engine_srand(unsigned int seed) { global_seed = seed ? seed : (unsigned int)time(NULL); srand(global_seed); }
static inline float frand01() { return (float)rand() / (float)RAND_MAX; }

/* Create engine */
EngineState *engine_create(int X, int Y, int D, int P) {
    EngineState *s = (EngineState*)malloc(sizeof(EngineState));
    if(!s) return NULL;
    s->X = X; s->Y = Y; s->D = D; s->P = P;
    size_t n = (size_t)X * Y * D * P;
    s->data = (float*)calloc(n, sizeof(float));
    s->total_steps = 0;
    s->total_energy = 0.0;
    return s;
}

void engine_free(EngineState *s) {
    if(!s) return;
    if(s->data) free(s->data);
    free(s);
}

/* Helper: compute global energy sum */
static double compute_energy(EngineState *s) {
    size_t n = (size_t)s->X * s->Y * s->D * s->P;
    double sum = 0.0;
    #pragma omp parallel for reduction(+:sum)
    for(size_t i=0;i<n;i++) sum += (double)s->data[i];
    return sum;
}

/* Inject random energy at expected inject_rate * cells per step */
void engine_inject(EngineState *s, float inject_strength, float inject_rate) {
    size_t cells = (size_t)s->X * s->Y;
    size_t expected = (size_t)ceil(inject_rate * (double)cells);
    for(size_t k=0;k<expected;k++) {
        int x = (int)(frand01() * s->X);
        int y = (int)(frand01() * s->Y);
        int d = (int)(frand01() * s->D);
        int p = (int)(frand01() * s->P);
        size_t idx = IDX(s,x,y,d,p);
        s->data[idx] += inject_strength * (0.5f + frand01()*0.5f);
    }
}

/* Apply one toppling pass. Return number of topplings and delta_energy. */
double engine_topple_pass(EngineState *s, float threshold, float dissipation, float phi_ethica, int *out_n_top) {
    int X=s->X, Y=s->Y, D=s->D, P=s->P;
    size_t total_cells = (size_t)X*Y*D*P;
    float *send = (float*)calloc(total_cells, sizeof(float));
    int n_top = 0;
    double total_excess = 0.0;

    // detect over-threshold and compute excess, mark send
    #pragma omp parallel for reduction(+:n_top,total_excess)
    for(int x=0;x<X;x++){
        for(int y=0;y<Y;y++){
            for(int d=0;d<D;d++){
                for(int p=0;p<P;p++){
                    size_t idx = IDX(s,x,y,d,p);
                    float val = s->data[idx];
                    if(val > threshold) {
                        float excess = val - threshold;
                        // toppling: split equally to neighbors (we choose 6 neighbors: 4 spatial + 2 dimensional)
                        float out_amt = excess / 6.0f;
                        // accumulate sends to neighbor positions (atomic via local buffer index)
                        // we will add to send buffer below (not atomic here — but we write to unique send index per sender)
                        send[idx] = out_amt;
                        n_top += 1;
                        total_excess += (double)excess;
                    }
                }
            }
        }
    }

    // Prepare new grid: subtract excess from toppling cells; neighbors receive sums
    // We'll use a temporary array for deltas
    float *delta = (float*)calloc(total_cells, sizeof(float));

    // First: reduce toppling cells to threshold
    #pragma omp parallel for
    for(int x=0;x<X;x++){
        for(int y=0;y<Y;y++){
            for(int d=0;d<D;d++){
                for(int p=0;p<P;p++){
                    size_t idx = IDX(s,x,y,d,p);
                    if(send[idx] > 0.0f) {
                        s->data[idx] = threshold;
                    }
                }
            }
        }
    }

    // Second: distribute send to neighbors (wrap toroidally)
    #pragma omp parallel for
    for(int x=0;x<X;x++){
        for(int y=0;y<Y;y++){
            for(int d=0;d<D;d++){
                for(int p=0;p<P;p++){
                    size_t idx = IDX(s,x,y,d,p);
                    float out_amt = send[idx];
                    if(out_amt <= 0.0f) continue;

                    // neighbors: (x+1,y),(x-1,y),(x,y+1),(x,y-1),(d+1),(d-1)
                    int x1 = (x+1)%X;
                    int x0 = (x-1+X)%X;
                    int y1 = (y+1)%Y;
                    int y0 = (y-1+Y)%Y;
                    int d1 = (d+1)%D;
                    int d0 = (d-1+D)%D;

                    size_t idx_x1 = IDX(s,x1,y,d,p);
                    size_t idx_x0 = IDX(s,x0,y,d,p);
                    size_t idx_y1 = IDX(s,x,y1,d,p);
                    size_t idx_y0 = IDX(s,x,y0,d,p);
                    size_t idx_d1 = IDX(s,x,y,d1,p);
                    size_t idx_d0 = IDX(s,x,y,d0,p);

                    #pragma omp atomic
                    delta[idx_x1] += out_amt;
                    #pragma omp atomic
                    delta[idx_x0] += out_amt;
                    #pragma omp atomic
                    delta[idx_y1] += out_amt;
                    #pragma omp atomic
                    delta[idx_y0] += out_amt;
                    #pragma omp atomic
                    delta[idx_d1] += out_amt;
                    #pragma omp atomic
                    delta[idx_d0] += out_amt;
                }
            }
        }
    }

    // Add delta to grid
    #pragma omp parallel for
    for(size_t i=0;i<total_cells;i++){
        s->data[i] += delta[i];
    }

    // apply dissipation proportional to total_excess
    double dissipated = 0.0;
    if(dissipation > 0.0 && total_excess > 0.0) {
        dissipated = total_excess * (double)dissipation;
        double tot = compute_energy(s);
        if(tot > 0.0) {
            double factor = 1.0 - (dissipated / tot);
            if(factor < 0.0) factor = 0.0;
            #pragma omp parallel for
            for(size_t i=0;i<total_cells;i++) s->data[i] *= (float)factor;
        }
    }

    // phi_ethica smoothing: small neighbor averaging
    if(phi_ethica > 0.0f && phi_ethica < 1.0f) {
        // apply lightweight smoothing: new = phi * cur + (1-phi) * spatial_avg
        // compute spatial avg across four spatial neighbors (keeping D,P)
        float *spatial_avg = (float*)calloc(total_cells, sizeof(float));
        #pragma omp parallel for
        for(int x=0;x<X;x++){
            for(int y=0;y<Y;y++){
                for(int d=0;d<D;d++){
                    for(int p=0;p<P;p++){
                        int x1 = (x+1)%X;
                        int x0 = (x-1+X)%X;
                        int y1 = (y+1)%Y;
                        int y0 = (y-1+Y)%Y;
                        size_t idx = IDX(s,x,y,d,p);
                        size_t idx_x1 = IDX(s,x1,y,d,p);
                        size_t idx_x0 = IDX(s,x0,y,d,p);
                        size_t idx_y1 = IDX(s,x,y1,d,p);
                        size_t idx_y0 = IDX(s,x,y0,d,p);
                        spatial_avg[idx] = (s->data[idx_x1] + s->data[idx_x0] + s->data[idx_y1] + s->data[idx_y0]) * 0.25f;
                    }
                }
            }
        }
        #pragma omp parallel for
        for(size_t i=0;i<total_cells;i++){
            s->data[i] = s->data[i]*phi_ethica + spatial_avg[i]*(1.0f - phi_ethica);
        }
        free(spatial_avg);
    }

    free(send);
    free(delta);
    if(out_n_top) *out_n_top = n_top;
    return -dissipated;
}

/* step: inject -> avalanche until stable or iteration cap */
double engine_step(EngineState *s, float threshold, float dissipation, float phi_ethica, int *out_n_topplings) {
    // for safety cap the avalanche iterations
    const int MAX_AVALANCHE_ITERS = 1000 + s->X * s->Y / 10;
    int total_top = 0;
    double delta_energy = 0.0;
    int iter = 0;
    // run avalanche passes until no cell > threshold
    while(iter < MAX_AVALANCHE_ITERS) {
        int n_top = 0;
        double dE = engine_topple_pass(s, threshold, dissipation, phi_ethica, &n_top);
        total_top += n_top;
        delta_energy += dE;
        // check any cell above threshold quickly (vector scan)
        bool any_over = false;
        size_t total_cells = (size_t)s->X * s->Y * s->D * s->P;
        #pragma omp parallel for reduction(|:any_over)
        for(size_t i=0;i<total_cells;i++){
            if(s->data[i] > threshold) any_over = true;
        }
        if(!any_over) break;
        iter++;
    }
    if(out_n_topplings) *out_n_topplings = total_top;
    return delta_energy;
}

/* simple FNV-1a 64-bit hash */
uint64_t engine_hash64(EngineState *s) {
    uint64_t hash = 1469598103934665603ULL;
    const uint64_t prime = 1099511628211ULL;
    size_t n = (size_t)s->X * s->Y * s->D * s->P;
    // interpret bytes of floats
    unsigned char *b = (unsigned char*)s->data;
    size_t nb = n * sizeof(float);
    for(size_t i=0;i<nb;i++){
        hash ^= (uint64_t)b[i];
        hash *= prime;
    }
    return hash;
}

/* convenience run loop */
int engine_run_loop(EngineState *s,
                    int steps,
                    float threshold,
                    float dissipation,
                    float phi_ethica,
                    float inject_rate,
                    float inject_strength,
                    double watchdog_E_max,
                    unsigned long watchdog_K_max) {
    if(!s) return -1;
    unsigned long total_top = 0;
    for(int st=0; st<steps; st++){
        // inject
        engine_inject(s, inject_strength, inject_rate);
        // step
        int n_top = 0;
        double dE = engine_step(s, threshold, dissipation, phi_ethica, &n_top);
        total_top += (unsigned long)n_top;
        s->total_steps++;
        s->total_energy = compute_energy(s);
        // watchdogs
        if(s->total_energy > watchdog_E_max) {
            fprintf(stderr, "Watchdog abort: energy exceeded %.6e at step %d\n", watchdog_E_max, st);
            return 2;
        }
        if(total_top > watchdog_K_max) {
            fprintf(stderr, "Watchdog abort: topplings exceeded %lu at step %d\n", watchdog_K_max, st);
            return 3;
        }
        // optional brief status
        if((st % 10) == 0) {
            printf("[step %d] energy=%.6f top_step=%d hash=%016llx\n", st, s->total_energy, n_top, (unsigned long long)engine_hash64(s));
        }
    }
    return 0;
}