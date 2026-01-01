#define _POSIX_C_SOURCE 200809L
#include "engine_nd.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <omp.h>

/* EngineND implementation (opaque) */

struct EngineND {
    int nd;         /* number of dimensions */
    int *dims;      /* sizes per dimension */
    size_t *strides;/* precomputed strides for indexing */
    int P;          /* pipelines (channels) per cell */
    size_t cells;   /* product of dims */
    size_t total;   /* cells * P */
    float *data;    /* flattened array */
    unsigned long total_steps;
    double total_energy;
    unsigned int seed;
};

/* Build-time date */
const char *engine_build_date(void) {
    return __DATE__ " " __TIME__;
}

/* runtime date ISO */
void engine_runtime_date(char *buf, size_t buflen) {
    time_t t = time(NULL);
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(buf, buflen, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

/* Simple RNG wrapper (not thread-safe for heavy parallelism, but seeded for reproducibility) */
static unsigned int g_seed = 123456789u;
void engine_srand_nd(unsigned int seed) {
    g_seed = seed ? seed : (unsigned int)time(NULL);
    srand(g_seed);
}

/* frand in [0,1) */
static inline float frand01(void) {
    return (float)rand() / ((float)RAND_MAX + 1.0f);
}

/* Helper: compute product and strides */
static int compute_strides(int nd, const int *dims, size_t *cells_out, size_t **strides_out) {
    if(nd <= 0 || dims == NULL) return -1;
    size_t *strides = (size_t*)malloc((size_t)nd * sizeof(size_t));
    if(!strides) return -1;
    size_t prod = 1;
    for(int i = nd-1; i >= 0; --i) {
        strides[i] = prod;
        prod *= (size_t)dims[i];
    }
    *cells_out = prod;
    *strides_out = strides;
    return 0;
}

/* Create engine */
EngineND *engine_create_nd(int nd, const int *dims, int P) {
    if(nd <= 0 || dims == NULL || P <= 0) return NULL;
    EngineND *e = (EngineND*)calloc(1, sizeof(EngineND));
    if(!e) return NULL;
    e->nd = nd;
    e->dims = (int*)malloc((size_t)nd * sizeof(int));
    memcpy(e->dims, dims, (size_t)nd * sizeof(int));
    e->P = P;
    if(compute_strides(nd, dims, &e->cells, &e->strides) != 0) {
        free(e->dims); free(e); return NULL;
    }
    e->total = e->cells * (size_t)P;
    /* guard against enormous allocations */
    if(e->total > (size_t)1e9) {
        fprintf(stderr, "Warning: engine_create_nd: total floats = %zu (may be very large)\n", e->total);
    }
    e->data = (float*)calloc(e->total, sizeof(float));
    if(!e->data) { free(e->dims); free(e->strides); free(e); return NULL; }
    e->total_steps = 0;
    e->total_energy = 0.0;
    e->seed = g_seed;
    return e;
}

void engine_free_nd(EngineND *e) {
    if(!e) return;
    free(e->dims);
    free(e->strides);
    if(e->data) free(e->data);
    free(e);
}

/* linear index for a multi-index coords[] of length nd and pipeline p */
static inline size_t linear_index(const EngineND *e, const int *coords, int p) {
    size_t idx = 0;
    for(int i=0;i<e->nd;i++) {
        idx += (size_t)coords[i] * e->strides[i];
    }
    idx = idx * (size_t)e->P + (size_t)p;
    return idx;
}

/* wrapper: total cells and total size */
size_t engine_cell_count_nd(const EngineND *e) { return e ? e->cells : 0; }
size_t engine_total_size_nd(const EngineND *e) { return e ? e->total : 0; }

/* compute energy sum */
static double compute_energy_nd(const EngineND *e) {
    double sum = 0.0;
    size_t n = e->total;
    #pragma omp parallel for reduction(+:sum)
    for(size_t i=0;i<n;i++) sum += (double)e->data[i];
    return sum;
}

/* Inject: choose expected inject_rate random cell positions and add inject_strength */
void engine_inject_nd(EngineND *e, float inject_strength, double inject_rate) {
    if(!e || inject_rate <= 0.0) return;
    size_t expected = (size_t)ceil(inject_rate);
    int *coords = (int*)malloc((size_t)e->nd * sizeof(int));
    for(size_t k=0;k<expected;k++) {
        for(int d=0; d<e->nd; d++) coords[d] = (int)(frand01() * e->dims[d]);
        int p = (int)(frand01() * e->P);
        size_t idx = linear_index(e, coords, p);
        e->data[idx] += inject_strength * (0.5f + frand01()*0.5f);
    }
    free(coords);
}

/* Generic neighbor iteration: for each axis, plus/minus neighbor (wrap/toroid) */
static inline void add_to_neighbor(float *delta, const EngineND *e, const int *coords, int axis, int sign, int p, float val) {
    int nc;
    int *ncoords = (int*)alloca((size_t)e->nd * sizeof(int));
    for(int i=0;i<e->nd;i++) ncoords[i] = coords[i];
    if(sign > 0) ncoords[axis] = (coords[axis] + 1) % e->dims[axis];
    else ncoords[axis] = (coords[axis] - 1 + e->dims[axis]) % e->dims[axis];
    size_t idx = linear_index(e, ncoords, p);
    #pragma omp atomic
    delta[idx] += val;
}

/* One topple pass: detect >threshold, compute excess, set sender->threshold and distribute to neighbors
 * We distribute equally to 2*nd neighbors across same pipeline p.
 */
static double engine_topple_pass_nd(EngineND *e, float threshold, float dissipation, float phi_ethica, unsigned long *out_n_top) {
    if(!e) return 0.0;
    size_t cells = e->cells;
    unsigned long n_top = 0;
    double total_excess = 0.0;

    /* allocate send flags per element; to minimize memory we will store send amount in temp array */
    float *send = (float*)calloc(e->total, sizeof(float));
    if(!send) return 0.0;
    /* scan for over-threshold */
    #pragma omp parallel reduction(+:n_top,total_excess)
    {
        int *coords = (int*)malloc((size_t)e->nd * sizeof(int));
        #pragma omp for
        for(size_t linear_cell = 0; linear_cell < cells; ++linear_cell) {
            /* compute multi-index coords from linear_cell and strides of cells (not including P) */
            size_t rem = linear_cell;
            for(int i=0;i<e->nd;i++) {
                coords[i] = (int)(rem / e->strides[i]);
                rem = rem % e->strides[i];
            }
            for(int p=0;p<e->P;p++) {
                size_t idx = linear_index(e, coords, p);
                float val = e->data[idx];
                if(val > threshold) {
                    float excess = val - threshold;
                    float out_amt = excess / (2.0f * e->nd); /* distribute equally to 2*nd neighbors */
                    send[idx] = out_amt;
                    total_excess += (double)excess;
                    n_top += 1;
                }
            }
        }
        free(coords);
    }

    /* apply: reset toppling cells to threshold and prepare delta */
    float *delta = (float*)calloc(e->total, sizeof(float));
    if(!delta) { free(send); return 0.0; }

    #pragma omp parallel
    {
        int *coords = (int*)malloc((size_t)e->nd * sizeof(int));
        #pragma omp for
        for(size_t linear_cell = 0; linear_cell < cells; ++linear_cell) {
            size_t rem = linear_cell;
            for(int i=0;i<e->nd;i++) {
                coords[i] = (int)(rem / e->strides[i]);
                rem = rem % e->strides[i];
            }
            for(int p=0;p<e->P;p++) {
                size_t idx = linear_index(e, coords, p);
                float out_amt = send[idx];
                if(out_amt > 0.0f) {
                    /* reset sender to threshold */
                    e->data[idx] = threshold;
                    /* distribute to neighbors along each axis +/- */
                    for(int axis=0; axis<e->nd; axis++) {
                        add_to_neighbor(delta, e, coords, axis, +1, p, out_amt);
                        add_to_neighbor(delta, e, coords, axis, -1, p, out_amt);
                    }
                }
            }
        }
        free(coords);
    }

    /* apply delta */
    size_t total = e->total;
    #pragma omp parallel for
    for(size_t i=0;i<total;i++) e->data[i] += delta[i];

    /* dissipation: remove fraction of total_excess globally */
    double dissipated = 0.0;
    if(dissipation > 0.0 && total_excess > 0.0) {
        dissipated = total_excess * (double)dissipation;
        double tot = compute_energy_nd(e);
        if(tot > 0.0) {
            double factor = 1.0 - (dissipated / tot);
            if(factor < 0.0) factor = 0.0;
            #pragma omp parallel for
            for(size_t i=0;i<total;i++) e->data[i] = (float)(e->data[i] * factor);
        }
    }

    /* phi_ethica smoothing: small local averaging across axes, simple implementation */
    if(phi_ethica > 0.0f && phi_ethica < 1.0f) {
        float *avg = (float*)calloc(e->total, sizeof(float));
        if(avg) {
            #pragma omp parallel
            {
                int *coords = (int*)malloc((size_t)e->nd * sizeof(int));
                #pragma omp for
                for(size_t linear_cell = 0; linear_cell < cells; ++linear_cell) {
                    size_t rem = linear_cell;
                    for(int i=0;i<e->nd;i++) {
                        coords[i] = (int)(rem / e->strides[i]);
                        rem = rem % e->strides[i];
                    }
                    for(int p=0;p<e->P;p++) {
                        float sum_neighbors = 0.0f;
                        for(int axis=0; axis<e->nd; axis++) {
                            int plus = (coords[axis] + 1) % e->dims[axis];
                            int minus = (coords[axis] - 1 + e->dims[axis]) % e->dims[axis];
                            coords[axis] = plus;
                            size_t idxp = linear_index(e, coords, p);
                            sum_neighbors += e->data[idxp];
                            coords[axis] = minus;
                            size_t idxm = linear_index(e, coords, p);
                            sum_neighbors += e->data[idxm];
                            /* restore */
                            coords[axis] = (coords[axis] + 1) % e->dims[axis];
                        }
                        size_t idx = linear_index(e, coords, p);
                        avg[idx] = sum_neighbors / (2.0f * e->nd);
                    }
                }
                free(coords);
            }
            #pragma omp parallel for
            for(size_t i=0;i<e->total;i++) e->data[i] = e->data[i] * phi_ethica + avg[i] * (1.0f - phi_ethica);
            free(avg);
        }
    }

    free(send);
    free(delta);
    if(out_n_top) *out_n_top = n_top;
    return -dissipated;
}

/* engine_step_nd: run avalanche passes until stable or cap */
double engine_step_nd(EngineND *e, float threshold, float dissipation, float phi_ethica, unsigned long *out_n_topplings) {
    if(!e) return 0.0;
    const int MAX_ITERS = 1000;
    unsigned long total_top = 0;
    double delta_energy = 0.0;
    for(int iter = 0; iter < MAX_ITERS; ++iter) {
        unsigned long n_top = 0;
        double dE = engine_topple_pass_nd(e, threshold, dissipation, phi_ethica, &n_top);
        total_top += n_top;
        delta_energy += dE;
        /* quick scan to detect any over-threshold */
        bool any_over = false;
        #pragma omp parallel for reduction(|:any_over)
        for(size_t i=0;i<e->total;i++) {
            if(e->data[i] > threshold) any_over = true;
        }
        if(!any_over) break;
    }
    if(out_n_topplings) *out_n_topplings = total_top;
    e->total_steps++;
    e->total_energy = compute_energy_nd(e);
    return delta_energy;
}

/* FNV-1a 64-bit hash of byte representation of data */
uint64_t engine_hash64_nd(EngineND *e) {
    uint64_t hash = 1469598103934665603ULL;
    const uint64_t prime = 1099511628211ULL;
    unsigned char *b = (unsigned char*)e->data;
    size_t nb = e->total * sizeof(float);
    for(size_t i=0;i<nb;i++) {
        hash ^= (uint64_t)b[i];
        hash *= prime;
    }
    return hash;
}

/* run loop convenience */
int engine_run_loop_nd(EngineND *e,
                       int steps,
                       float threshold,
                       float dissipation,
                       float phi_ethica,
                       double inject_rate,
                       float inject_strength,
                       double watchdog_E_max,
                       unsigned long watchdog_K_max,
                       int verbose) {
    if(!e) return -1;
    unsigned long total_top_all = 0;
    for(int s=0;s<steps;s++) {
        engine_inject_nd(e, inject_strength, inject_rate);
        unsigned long n_top = 0;
        engine_step_nd(e, threshold, dissipation, phi_ethica, &n_top);
        total_top_all += n_top;
        double energy = compute_energy_nd(e);
        e->total_energy = energy;
        if(verbose && (s % 10 == 0)) {
            char buf[64];
            engine_runtime_date(buf, sizeof(buf));
            printf("[%s] step=%d energy=%.6f top_this_step=%lu hash=%016llx\n", buf, s, energy, n_top, (unsigned long long)engine_hash64_nd(e));
        }
        if(energy > watchdog_E_max) {
            fprintf(stderr, "watchdog abort: energy exceeded %.6e at step %d\n", watchdog_E_max, s);
            return 2;
        }
        if(total_top_all > watchdog_K_max) {
            fprintf(stderr, "watchdog abort: topplings exceeded %lu at step %d\n", watchdog_K_max, s);
            return 3;
        }
    }
    return 0;
}

/* small debug dump */
void engine_dump_region_nd(const EngineND *e, int max_cells) {
    if(!e) return;
    size_t n = e->cells;
    size_t to = (size_t) (max_cells > 0 && (size_t)max_cells < n ? max_cells : n);
    printf("EngineND dump (first %zu cells) P=%d\n", to, e->P);
    int *coords = (int*)malloc((size_t)e->nd * sizeof(int));
    for(size_t linear_cell = 0; linear_cell < to; ++linear_cell) {
        size_t rem = linear_cell;
        for(int i=0;i<e->nd;i++) {
            coords[i] = (int)(rem / e->strides[i]);
            rem = rem % e->strides[i];
        }
        printf("cell %zu coords [", linear_cell);
        for(int i=0;i<e->nd;i++) {
            printf("%d%s", coords[i], i+1<e->nd ? "," : "");
        }
        printf("] : ");
        for(int p=0;p<e->P;p++) {
            size_t idx = linear_index(e, coords, p);
            printf("%.3f%s", e->data[idx], p+1<e->P ? "," : "");
        }
        printf("\n");
    }
    free(coords);
}