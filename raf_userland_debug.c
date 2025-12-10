// raf_userland_debug.c
// Draft – UserLAnd Debug Matrix Core
// C11, no GC, low-level only.

#include "raf_core.h"   // Reuse AbsoluteMatrix, RNG, CRC helpers where possible.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>

// -------------------------------
// Local constants and types
// -------------------------------

#define R_MAX_ROWS   64u     // Matrix rows (time windows / signals)
#define R_STATE_COLS 4u      // CPU, IO, NET, RISK (linear combo)
#define R_JSON_BUF   512u    // Output buffer size for JSON stats
#define R_TAG_LEN    32u     // Small tag buffers
#define R_STAT_LEN   4u      // mean, peak, z-score, energy
#define R_TS_DELTA   1.0f    // Fixed time delta (seconds) for resample

typedef struct {
    float m[R_MAX_ROWS][R_STATE_COLS]; // Main matrix (metrics)
    uint32_t row_counts[R_MAX_ROWS];   // Samples per resampled slice
    uint32_t r_used;                   // Number of active rows
    uint32_t c_used;                   // Number of active cols
    uint32_t severity;                 // 0=ok,1=warn,2=critical
    uint32_t crc_state;                // CRC over matrix
    uint64_t t_epoch;                  // Epoch snapshot
    float t0;                          // Base timestamp for resampling
    int has_t0;                        // Flag to check base timestamp
} RField;

// -------------------------------
// Small helpers (no GC, no globals)
// -------------------------------

// Map value to [0,1] (simple normalization).
static float n01(float v, float min, float max) {
    if (max <= min) {
        return 0.0f;
    }
    float x = (v - min) / (max - min);
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    return x;
}

// Compute a simple "energy" metric of the matrix (risk column).
static float e(RField *f) {
    float acc = 0.0f;
    for (uint32_t i = 0; i < f->r_used; ++i) {
        float v = f->m[i][3];
        acc += v * v;
    }
    return acc;
}

// Update CRC of the matrix using raf_fast_crc32.
static void k(RField *f) {
    f->crc_state = raf_fast_crc32(0u, f->m,
        (size_t)(sizeof(float) * f->r_used * f->c_used));
}

// -------------------------------
// Core step: analyze and classify
// -------------------------------

// Interpret matrix as debug "frame" and derive severity.
static void s(RField *f) {
    // In a real build, thresholds come from config.
    const float warn_e = 64.0f;
    const float crit_e = 256.0f;

    float energy = e(f);

    if (energy >= crit_e) {
        f->severity = 2; // critical
    } else if (energy >= warn_e) {
        f->severity = 1; // warning
    } else {
        f->severity = 0; // ok
    }

    k(f);
}

// -------------------------------
// Input mapping (from simple CSV-like lines)
// -------------------------------

// Parse a single line "ts,cpu,io,net" and resample into fixed delta slots.
static int v(const char *line, RField *f) {
    if (!line || !f) return -1;

    // Strict parsing: we do not trust input.
    // Example: "12.5, 35.0, 120.0, 15.0" (ts, CPU%, IO MB/s, NET MB/s)
    float ts = 0.0f, cpu = 0.0f, io = 0.0f, net = 0.0f;
    int n = sscanf(line, "%f , %f , %f , %f", &ts, &cpu, &io, &net);
    if (n < 3) {
        // At least ts, cpu, io are required.
        return -3;
    }

    if (!f->has_t0) {
        f->t0 = ts;
        f->has_t0 = 1;
    }

    float rel = ts - f->t0;
    if (rel < 0.0f) {
        rel = 0.0f; // clamp backwards drift
    }

    uint32_t idx = (uint32_t)(rel / R_TS_DELTA + 0.5f); // nearest slot
    if (idx >= R_MAX_ROWS) {
        return -4; // out of buffer
    }

    while (f->r_used <= idx && f->r_used < R_MAX_ROWS) {
        memset(f->m[f->r_used], 0, sizeof(float) * R_STATE_COLS);
        f->row_counts[f->r_used] = 0u;
        f->r_used++;
    }

    // Normalize to [0,1] in a simple way; real thresholds should be configurable.
    float norm_cpu = n01(cpu, 0.0f, 100.0f);   // CPU %
    float norm_io  = n01(io,  0.0f, 1000.0f);  // IO throughput
    float norm_net = n01(net, 0.0f, 1000.0f);  // Network throughput

    f->m[idx][0] += norm_cpu;
    f->m[idx][1] += norm_io;
    f->m[idx][2] += norm_net;
    f->row_counts[idx] += 1u;
    f->c_used = R_STATE_COLS;
    return 0;
}

// -------------------------------
// Statistics output (minimal JSON or binary vector)
// -------------------------------

static void stats(const RField *f, float out[R_STAT_LEN]) {
    if (!out) return;

    float mean = 0.0f;
    float peak = 0.0f;
    float energy = 0.0f;

    for (uint32_t i = 0; i < f->r_used; ++i) {
        float v = f->m[i][3];
        mean += v;
        energy += v * v;
        if (v > peak) peak = v;
    }

    mean = (f->r_used > 0) ? (mean / (float)f->r_used) : 0.0f;

    float var = 0.0f;
    for (uint32_t i = 0; i < f->r_used; ++i) {
        float d = f->m[i][3] - mean;
        var += d * d;
    }

    float std = (f->r_used > 1u) ? sqrtf(var / (float)(f->r_used - 1u)) : 0.0f;
    float last = (f->r_used > 0) ? f->m[f->r_used - 1u][3] : 0.0f;
    float z = (std > 0.0f) ? ((last - mean) / std) : 0.0f;

    out[0] = mean;
    out[1] = peak;
    out[2] = z;
    out[3] = energy;
}

static void j_stats(const RField *f, const float *stats_vec, char *out, size_t out_len, const char *tag) {
    if (!out || out_len == 0 || !stats_vec) return;

    uint32_t sev = f->severity;
    if (!tag) tag = "userland_debug";

    int w = snprintf(out, out_len,
        "{""\"tag\":\"%s\",""\"severity\":%u,""\"rows\":%u,""\"cols\":%u,""\"crc\":%u,""\"epoch\":%llu,""\"stats\":[%.6f,%.6f,%.6f,%.6f]}",
        tag,
        sev,
        f->r_used,
        f->c_used,
        f->crc_state,
        (unsigned long long)f->t_epoch,
        stats_vec[0],
        stats_vec[1],
        stats_vec[2],
        stats_vec[3]
    );

    if (w < 0 || (size_t)w >= out_len) {
        if (out_len > 2u) {
            out[0] = '{';
            out[1] = '}';
            out[2] = '\0';
        }
    }
}

// -------------------------------
// CLI entry point
// -------------------------------

int main(int argc, char **argv) {
    int binary_out = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--bin") == 0 || strcmp(argv[i], "-b") == 0) {
            binary_out = 1;
        }
    }

    RField f;
    memset(&f, 0, sizeof(f));

    // Time snapshot for traceability (ISO 27001 logging).
    f.t_epoch = (uint64_t)time(NULL);

    // Strict stdin processing: CSV lines from shell or another tool.
    char line[256];
    while (f.r_used < R_MAX_ROWS && fgets(line, sizeof(line), stdin)) {
        if (line[0] == '#' || line[0] == '\n') {
            continue; // ignore comments / empty
        }
        int r = v(line, &f);
        if (r != 0) {
            // Security choice: do not print raw line to avoid leaking secrets.
            fprintf(stderr, "[RDBG] parse_error code=%d\n", r);
        }
    }

    // Fill missing slices with the last known state to keep buffer continuous.
    float last_state[R_STATE_COLS] = {0};
    for (uint32_t i = 0; i < f.r_used; ++i) {
        if (f.row_counts[i] > 0u) {
            for (uint32_t c = 0; c < 3u; ++c) {
                f.m[i][c] /= (float)f.row_counts[i];
                last_state[c] = f.m[i][c];
            }
        } else {
            for (uint32_t c = 0; c < 3u; ++c) {
                f.m[i][c] = last_state[c];
            }
        }

        // Linear combination for combined risk (weights tuned for balance).
        const float w_cpu = 0.5f, w_io = 0.3f, w_net = 0.2f;
        f.m[i][3] = (f.m[i][0] * w_cpu) + (f.m[i][1] * w_io) + (f.m[i][2] * w_net);
    }

    s(&f); // analyze matrix

    float stats_vec[R_STAT_LEN] = {0};
    stats(&f, stats_vec);

    if (binary_out) {
        fwrite(stats_vec, sizeof(float), R_STAT_LEN, stdout);
    } else {
        char json[R_JSON_BUF];
        j_stats(&f, stats_vec, json, sizeof(json), "raf_userland_matrix");

        // Always one single JSON object to stdout: safe for pipes.
        fputs(json, stdout);
        fputc('\n', stdout);
    }

    return 0;
}
