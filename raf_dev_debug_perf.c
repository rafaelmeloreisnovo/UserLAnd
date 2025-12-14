// raf_dev_debug_perf.c
// Draft – RAFAELIA Developer Mode low-level performance matrix (ISO/NIST compliant)
// English comments per function block; no external deps; C11 minimal footprint.
#define _POSIX_C_SOURCE 200809L

#include "raf_core.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// -------------------------------
// Constants and scoped limits
// -------------------------------
#define D_ROWS 64u      // time slices
#define D_COLS 8u       // metrics per slice
#define D_JSON 12288u   // JSON buffer (covers worst-case 64x8 matrix with headroom)
#define D_TAG  32u      // label length
#define D_TOL  1e-3f     // fixed tolerance for numeric equivalence
#define D_WARN_E 96.0f   // warning energy threshold
#define D_CRIT_E 384.0f  // critical energy threshold

// -------------------------------
// Profiling toggle and budget
// -------------------------------
typedef struct {
    uint8_t  enabled;
    uint64_t threshold_ns;   // hard limit for internal overhead
    uint64_t last_overhead;  // measured overhead for diagnostics
} ProfileMode;

static uint64_t nsec_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static uint64_t measure_timer_overhead(void) {
    const int samples = 256;
    uint64_t acc = 0;
    for (int i = 0; i < samples; ++i) {
        uint64_t a = nsec_now();
        uint64_t b = nsec_now();
        acc += (b - a);
    }
    return acc / (uint64_t)samples;
}

static int profile_guard(ProfileMode *p) {
    if (!p || !p->enabled) {
        return 0;
    }
    p->last_overhead = measure_timer_overhead();
    if (p->last_overhead > p->threshold_ns) {
        fprintf(stderr,
                "[RAF_DEVPERF] profiling_overhead=%" PRIu64 "ns threshold=%" PRIu64 "ns (abort)\n",
                p->last_overhead, p->threshold_ns);
        return -1;
    }
    return 0;
}

// -------------------------------
// Time-series block layout (row-major, SIMD friendly)
// -------------------------------
typedef struct {
    uint32_t len;          // total elements stored (rows * cols)
    uint32_t stride;       // element stride in bytes
    uint64_t ts_start;     // epoch base for first sample
    uint64_t ts_delta;     // delta between rows (nanoseconds)
} TSBlockHeader;

typedef struct {
    TSBlockHeader header;          // [len | stride | ts_start | ts_delta]
    union {                       // contiguous row-major payload, 64B aligned
        float    f32[D_ROWS * D_COLS];
        double   f64[D_ROWS * D_COLS];
        int16_t  i16[D_ROWS * D_COLS];
    } payload;
} TSBlock;

#if defined(__GNUC__) || defined(__clang__)
typedef TSBlock TSBlockAligned __attribute__((aligned(64)));
#else
typedef __declspec(align(64)) TSBlock TSBlockAligned;
#endif

// -------------------------------
// Data field and permissions snapshot
// -------------------------------
typedef struct {
    TSBlockAligned series;   // aligned time-series buffer
    uint32_t r_used;         // active rows
    uint32_t c_used;         // active cols
    uint32_t severity;       // 0 ok / 1 warn / 2 critical
    uint32_t crc_state;      // crc over matrix
    uint64_t t_epoch;        // epoch seconds
    uint8_t perm_dev;        // developer mode asserted
    uint8_t perm_p2p;        // P2P allowed (tor-like overlay)
    uint8_t perm_io;         // IO read allowed
    uint8_t perm_net;        // network monitor allowed
    char tag[D_TAG];         // label for JSON
} DField;

// -------------------------------
// Clamp helpers – deterministic, no heap
// -------------------------------
static float c01(float v, float lo, float hi) {
    if (hi <= lo) return 0.0f; // mitigation: avoid div0
    float x = (v - lo) / (hi - lo);
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    return x;
}

// -------------------------------
// Contiguous-friendly scalar kernels
// -------------------------------
static float k_sum(const float *in, uint32_t n) {
    float acc = 0.0f;
    for (uint32_t i = 0; i < n; ++i) {
        acc += in[i];
    }
    return acc;
}

static float k_mean(float sum, uint32_t n) {
    return (n == 0u) ? 0.0f : sum / (float)n;
}

static void k_minmax(const float *in, uint32_t n, float *mn, float *mx) {
    if (n == 0u || !mn || !mx) {
        if (mn) *mn = 0.0f;
        if (mx) *mx = 0.0f;
        return;
    }
    float lo = in[0];
    float hi = in[0];
    for (uint32_t i = 1u; i < n; ++i) {
        float v = in[i];
        lo = (v < lo) ? v : lo;
        hi = (v > hi) ? v : hi;
    }
    *mn = lo;
    *mx = hi;
}

static float k_variance(const float *in, uint32_t n, float mean) {
    if (n == 0u) return 0.0f;
    float acc = 0.0f;
    for (uint32_t i = 0; i < n; ++i) {
        float d = in[i] - mean;
        acc += d * d;
    }
    return acc / (float)n;
}

static void k_conv1d(const float *in, uint32_t n,
                     const float *k, uint32_t klen,
                     float *out) {
    if (!in || !k || !out || n == 0u || klen == 0u) {
        return;
    }
    uint32_t half = klen / 2u;
    for (uint32_t i = 0; i < n; ++i) {
        float acc = 0.0f;
        for (uint32_t j = 0; j < klen; ++j) {
            int32_t pos = (int32_t)i + (int32_t)j - (int32_t)half;
            if (pos >= 0 && (uint32_t)pos < n) {
                acc += in[(uint32_t)pos] * k[j];
            }
        }
        out[i] = acc;
    }
}

// -------------------------------
// Baseline permissions and tag
// -------------------------------
static void p(DField *f) {
    if (!f) return;
    f->perm_dev = 1u;   // developer mode required (Zero Trust precondition)
    f->perm_p2p = 1u;   // allow P2P handoff when toggled in dev options
    f->perm_io  = 1u;   // allow /proc sampling in ethical mode
    f->perm_net = 1u;   // allow metadata-only network metrics
    snprintf(f->tag, sizeof(f->tag), "raf_devperf");

    f->series.header.len     = 0u;
    f->series.header.stride  = (uint32_t)sizeof(float);
    f->series.header.ts_start= 0u;
    f->series.header.ts_delta= 0u;
}

// -------------------------------
// Risk scoring based on aggregated energy
// -------------------------------
static uint32_t clamp_used(uint32_t v, uint32_t max_v) {
    return (v <= max_v) ? v : max_v;
}

static void clamp_dims(DField *f) {
    if (!f) return;
    f->r_used = clamp_used(f->r_used, D_ROWS);
    f->c_used = clamp_used(f->c_used, D_COLS);
}

static float e(DField *f) {
    clamp_dims(f);
    uint32_t rows = f->r_used;
    uint32_t cols = f->c_used;
    const float *m = f->series.payload.f32;
    uint32_t total = rows * cols;
    float acc = 0.0f;
    for (uint32_t idx = 0; idx < total; ++idx) {
        float v = m[idx];
        acc += v * v; // energy-like metric; stable and cheap
    }
    return acc;
}

static float e_ref(const DField *f) {
    double acc = 0.0;
    for (uint32_t i = 0; i < f->r_used; ++i) {
        for (uint32_t j = 0; j < f->c_used; ++j) {
            double v = (double)f->series.payload.f32[(size_t)i * f->c_used + j];
            acc += v * v;
        }
    }
    return (float)acc;
}

static uint32_t sev_from_energy(float energy) {
    if (energy >= D_CRIT_E) {
        return 2u;
    }
    if (energy >= D_WARN_E) {
        return 1u;
    }
    return 0u;
}

static void k(DField *f) {
    clamp_dims(f);
    size_t bytes = (size_t)(sizeof(float) * f->r_used * f->c_used);
    f->crc_state = raf_fast_crc32(0u, f->series.payload.f32, bytes);
}

static void finalize_ts_block(DField *f, uint64_t delta_ns) {
    if (!f) return;
    f->series.header.stride = (uint32_t)sizeof(float);
    f->series.header.len = f->r_used * f->c_used;
    if (f->series.header.ts_start == 0u) {
        f->series.header.ts_start = f->t_epoch * 1000000000ull; // epoch to ns
    }
    if (f->series.header.ts_delta == 0u) {
        f->series.header.ts_delta = delta_ns;
    }
}

// -------------------------------
// Metric vector mapping (cpu,mem,io,net,jank)
// -------------------------------
static void m_stats(const DField *f,
                    float *mean_out,
                    float *var_out,
                    float *min_out,
                    float *max_out,
                    float *conv_tail) {
    if (!f) return;

    static const uint32_t cols[] = {0u, 1u, 2u, 3u, 6u};
    const float smooth_k[] = {0.25f, 0.50f, 0.25f};
    float scratch[D_ROWS];
    float conv[D_ROWS];

    uint32_t samples = clamp_used(f->r_used, D_ROWS);
    uint32_t col_count = clamp_used(f->c_used, D_COLS);
    if (samples == 0u || col_count == 0u) {
        return;
    }

    for (uint32_t m = 0; m < (sizeof(cols) / sizeof(cols[0])); ++m) {
        if (cols[m] >= col_count) {
            continue; // skip unavailable column
        }
        for (uint32_t i = 0; i < samples; ++i) {
            scratch[i] = f->series.payload.f32[(size_t)i * f->c_used + cols[m]]; // contiguous column slice
        }

        float sum = k_sum(scratch, samples);
        float mean = k_mean(sum, samples);
        float variance = k_variance(scratch, samples, mean);
        float mn = 0.0f, mx = 0.0f;
        k_minmax(scratch, samples, &mn, &mx);
        k_conv1d(scratch, samples, smooth_k,
                 (uint32_t)(sizeof(smooth_k) / sizeof(smooth_k[0])), conv);

        if (mean_out) mean_out[m] = mean;
        if (var_out) var_out[m] = variance;
        if (min_out) min_out[m] = mn;
        if (max_out) max_out[m] = mx;
        if (conv_tail) conv_tail[m] = (samples == 0u) ? 0.0f : conv[samples - 1u];
    }
}

static void s(DField *f) {
    float energy = e(f);
    f->severity = sev_from_energy(energy);
    const float warn_e = 96.0f;   // conservative threshold for warning
    const float crit_e = 384.0f;  // stricter for critical

    float mean_out[5] = {0};
    float var_out[5] = {0};
    float min_out[5] = {0};
    float max_out[5] = {0};
    float conv_tail[5] = {0};

    m_stats(f, mean_out, var_out, min_out, max_out, conv_tail);

    for (uint32_t i = 0; i < 5u; ++i) {
        energy += mean_out[i] + var_out[i] + (max_out[i] - min_out[i]) + conv_tail[i];
    }
    if (energy >= crit_e) {
        f->severity = 2u;
    } else if (energy >= warn_e) {
        f->severity = 1u;
    } else {
        f->severity = 0u;
    }
    k(f);
}

// -------------------------------
// Parse CSV rows (cpu,mem,io,net,gpu,temp,jank,entropy)
// -------------------------------
static int v(const char *line, DField *f) {
    if (!line || !f) return -1;
    if (f->r_used >= D_ROWS) return -2;

    float cpu = 0.0f, mem = 0.0f, io = 0.0f, net = 0.0f;
    float gpu = 0.0f, tmp = 0.0f, jank = 0.0f, ent = 0.0f;
    int n = sscanf(line, "%f , %f , %f , %f , %f , %f , %f , %f",
                   &cpu, &mem, &io, &net, &gpu, &tmp, &jank, &ent);
    if (n < 4) {
        return -3; // require at least cpu,mem,io,net to be meaningful
    }

    uint32_t r = f->r_used;
    f->c_used = 8u;

    // Decode /proc or dumpsys rows directly into the aligned row-major buffer
    // to keep SIMD/NEON loads contiguous.
    float *row = f->series.payload.f32 + ((size_t)r * f->c_used);

    row[0] = c01(cpu, 0.0f, 100.0f);
    row[1] = c01(mem, 0.0f, 8192.0f);
    row[2] = c01(io,  0.0f, 2000.0f);
    row[3] = c01(net, 0.0f, 1000.0f);
    row[4] = c01(gpu, 0.0f, 100.0f);
    row[5] = c01(tmp, 0.0f, 110.0f);
    row[6] = c01(jank,0.0f, 120.0f);
    row[7] = c01(ent, 0.0f, 10.0f);

    f->r_used++;
    clamp_dims(f);
    f->series.header.len = f->r_used * f->c_used;
    return 0;
}

// -------------------------------
// Numeric equivalence guard (old vs. new paths)
// -------------------------------
static int q(const DField *f, float tol) {
    DField ref;
    memcpy(&ref, f, sizeof(ref));

    float energy_old = e_ref(&ref);
    float energy_new = e((DField *)&ref);
    float delta = fabsf(energy_old - energy_new);
    uint32_t sev_old = sev_from_energy(energy_old);
    uint32_t sev_new = sev_from_energy(energy_new);

    if (delta > tol || sev_old != sev_new) {
        fprintf(stderr,
                "[RAF_DEVPERF] equivalence_failed delta=%.6f sev_old=%u sev_new=%u\n",
                delta, sev_old, sev_new);
        return -1;
    }
    return 0;
}

// -------------------------------
// JSON writer (W3C/JSON, RFC 8259 safe subset)
// -------------------------------
static int j(const DField *f, char *out, size_t out_len) {
    if (!f || !out || out_len == 0) {
        return -1; // invalid args
    }

    uint32_t rows = clamp_used(f->r_used, D_ROWS);
    uint32_t cols = clamp_used(f->c_used, D_COLS);
    uint32_t sev = f->severity;

    /*
     * Predict total length to avoid ambiguous partial writes (ISO/IEEE/NIST defensive).
     * snprintf(NULL,0,...) is C11-compliant and keeps footprint minimal.
     */
    int prefix = snprintf(NULL, 0,
        "{"
        "\"tag\":\"%s\","
        "\"severity\":%u,"
        "\"rows\":%u,"
        "\"cols\":%u,"
        "\"crc\":%u,"
        "\"epoch\":%llu,"
        "\"perm\":{\"dev\":%u,\"p2p\":%u,\"io\":%u,\"net\":%u},"
        "\"p2p_overlay\":\"tor-metadata-only\","
        "\"matrix\":[",
        f->tag,
        sev,
        rows,
        cols,
        f->crc_state,
        (unsigned long long)f->t_epoch,
        f->perm_dev,
        f->perm_p2p,
        f->perm_io,
        f->perm_net
    );
    if (prefix < 0) {
        out[0] = '\0';
        return -2; // encoding failure
    }

    size_t need = (size_t)prefix;
    for (uint32_t i = 0; i < rows; ++i) {
        need += 1u; // '['
        for (uint32_t c = 0; c < cols; ++c) {
            int cell = snprintf(NULL, 0,
                                (c + 1u < cols) ? "%.6f," : "%.6f",
                                f->series.payload.f32[(size_t)i * f->c_used + c]);
            if (cell < 0) {
                out[0] = '\0';
                return -2;
            }
            need += (size_t)cell;
        }
        need += 1u; // ']'
        if (i + 1u < rows) {
            need += 1u; // ',' between rows
        }
    }
    need += 2u; // closing ]}

    if (need + 1u > out_len) { // +1 for NUL
        out[0] = '\0';
        return 1; // overflow prevented
    }

    int w = snprintf(out, out_len,
        "{"
        "\"tag\":\"%s\","
        "\"severity\":%u,"
        "\"rows\":%u,"
        "\"cols\":%u,"
        "\"crc\":%u,"
        "\"epoch\":%llu,"
        "\"perm\":{\"dev\":%u,\"p2p\":%u,\"io\":%u,\"net\":%u},"
        "\"p2p_overlay\":\"tor-metadata-only\","
        "\"matrix\":[",
        f->tag,
        sev,
        rows,
        cols,
        f->crc_state,
        (unsigned long long)f->t_epoch,
        f->perm_dev,
        f->perm_p2p,
        f->perm_io,
        f->perm_net
    );
    if (w < 0 || (size_t)w >= out_len) {
        out[0] = '\0';
        return -2;
    }

    size_t pos = (size_t)w;
    for (uint32_t i = 0; i < rows; ++i) {
        out[pos++] = '[';
        for (uint32_t c = 0; c < cols; ++c) {
            int z = snprintf(out + pos, out_len - pos,
                             (c + 1u < cols) ? "%.6f," : "%.6f",
                             f->series.payload.f32[(size_t)i * f->c_used + c]);
            if (z < 0 || (size_t)z >= (out_len - pos)) {
                out[0] = '\0';
                return -2;
            }
            pos += (size_t)z;
        }
        out[pos++] = ']';
        if (i + 1u < rows) {
            out[pos++] = ',';
        }
    }

    out[pos++] = ']';
    out[pos++] = '}';
    out[pos] = '\0';
    return 0;
}

// -------------------------------
// In-process serializer guardrail (table driven)
// -------------------------------
static int h(void) {
    typedef struct { size_t buf; uint32_t rows; uint32_t cols; int expect; } JCase;
    static const JCase cases[] = {
        {8u, 1u, 2u, 1},      // tiny buffer must fail
        {64u, 4u, 4u, 1},     // mid buffer with multiple rows should fail fast
        {D_JSON, 2u, 3u, 0}   // nominal case should pass
    };

    for (size_t idx = 0; idx < (sizeof(cases) / sizeof(cases[0])); ++idx) {
        DField f;
        memset(&f, 0, sizeof(f));
        p(&f);
        f.r_used = cases[idx].rows;
        f.c_used = cases[idx].cols;
        f.t_epoch = 1u;
        for (uint32_t r = 0; r < f.r_used && r < D_ROWS; ++r) {
            for (uint32_t c = 0; c < f.c_used && c < D_COLS; ++c) {
                f.series.payload.f32[(size_t)r * f.c_used + c] =
                    0.123456f * (float)(r + 1u + c);
            }
        }
        finalize_ts_block(&f, 1000000000ull);
        char tmp[D_JSON];
        memset(tmp, 0, sizeof(tmp));
        size_t limit = (cases[idx].buf < sizeof(tmp)) ? cases[idx].buf : sizeof(tmp);
        int status = j(&f, tmp, limit);
        if (status != cases[idx].expect) {
            return -1; // harness failure
        }
    }
    return 0;
}

// -------------------------------
// Synthetic microbenchmarks (no external deps)
// -------------------------------
static void g(char *line, size_t len, uint32_t idx) {
    // deterministic synthetic row, covers full metric set
    float cpu = 25.0f + (float)(idx % 50u);
    float mem = 512.0f + 8.0f * (float)(idx % 32u);
    float io  = 100.0f + 5.5f * (float)(idx % 40u);
    float net = 10.0f * (float)(idx % 60u);
    float gpu = 5.0f * (float)(idx % 10u);
    float tmp = 45.0f + (float)(idx % 8u);
    float jnk = 2.0f * (float)(idx % 30u);
    float ent = 0.5f * (float)(idx % 12u);
    snprintf(line, len, "%0.2f , %0.2f , %0.2f , %0.2f , %0.2f , %0.2f , %0.2f , %0.2f",
             cpu, mem, io, net, gpu, tmp, jnk, ent);
}

static void b(ProfileMode *profile) {
    DField f;
    memset(&f, 0, sizeof(f));
    p(&f);

    char line[160];
    uint64_t t_parse = 0;
    uint64_t t_calc = 0;
    uint64_t t_json = 0;

    for (uint32_t i = 0; i < D_ROWS; ++i) {
        g(line, sizeof(line), i);
        uint64_t t0 = nsec_now();
        (void)v(line, &f);
        t_parse += nsec_now() - t0;
    }

    uint64_t t0 = nsec_now();
    s(&f);
    t_calc = nsec_now() - t0;

    char json[D_JSON];
    t0 = nsec_now();
    (void)j(&f, json, sizeof(json));
    t_json = nsec_now() - t0;

    fprintf(stderr,
            "[RAF_DEVPERF] microbench parse=%" PRIu64 "ns calc=%" PRIu64 "ns json=%" PRIu64 "ns\n",
            t_parse, t_calc, t_json);

    if (profile && profile_guard(profile) != 0) {
        exit(2);
    }

    if (q(&f, D_TOL) != 0) {
        exit(3);
    }
    return;
}

// -------------------------------
// Main entry – pipeline friendly
// -------------------------------
int main(int argc, char **argv) {
    ProfileMode profile = {0, 200000ull, 0}; // ~0.2 ms budget for timing overhead
    int run_bench = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--profile") == 0) {
            profile.enabled = 1u;
        } else if (strcmp(argv[i], "--microbench") == 0) {
            run_bench = 1;
        } else if (strncmp(argv[i], "--profile-threshold=", 21) == 0) {
            profile.threshold_ns = (uint64_t)strtoull(argv[i] + 21, NULL, 10);
        }
    }

    if (profile.enabled && profile_guard(&profile) != 0) {
        return 2; // profiling overhead too high
    }

    if (h() != 0) {
        fprintf(stderr, "[RAF_DEVPERF] serializer_selftest_failed\n");
    }

    if (run_bench) {
        b(&profile);
    }

    if (h() != 0) {
        fprintf(stderr, "[RAF_DEVPERF] serializer_selftest_failed\n");
    }

    DField f;
    memset(&f, 0, sizeof(f));
    p(&f);
    f.t_epoch = (uint64_t)time(NULL);
    f.c_used = D_COLS;

    char line[256];
    while (f.r_used < D_ROWS && fgets(line, sizeof(line), stdin)) {
        if (line[0] == '#' || line[0] == '\n') {
            continue; // skip comments/blank for stability
        }
        int r = v(line, &f);
        if (r != 0) {
            fprintf(stderr, "[RAF_DEVPERF] parse_error=%d\n", r);
        }
    }

    if (q(&f, D_TOL) != 0) {
        return 2;
    }

    clamp_dims(&f);
    finalize_ts_block(&f, 1000000000ull);
    s(&f);

    char json[D_JSON];
    int js = j(&f, json, sizeof(json));
    if (js != 0) {
        fprintf(stderr, "[RAF_DEVPERF] serialize_error=%d buffer=%zu rows=%u cols=%u\n",
                js, sizeof(json), f.r_used, f.c_used);
        fputs("{}\n", stdout);
        return 1;
    }

    fputs(json, stdout);
    fputc('\n', stdout);
    return 0;
}
