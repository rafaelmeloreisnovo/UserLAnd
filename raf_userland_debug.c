// raf_userland_debug.c
// Draft – UserLAnd Debug Matrix Core
// C11, no GC, low-level only.

#define _POSIX_C_SOURCE 200809L

#include "raf_core.h"   // Reuse AbsoluteMatrix, RNG, CRC helpers where possible.
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>

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
// Minimal integer to string (base 10). Returns length written (without null).
static size_t u32_to_str(uint32_t v, char *out, size_t cap) {
    if (cap == 0u) {
        return 0u;
    }
    char tmp[16];
    size_t idx = 0u;
    do {
        tmp[idx++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v != 0u && idx < sizeof(tmp));

    size_t pos = 0u;
    while (idx > 0u && pos + 1u < cap) {
        out[pos++] = tmp[--idx];
    }
    out[pos] = '\0';
    return pos;
}

static size_t u64_to_str(uint64_t v, char *out, size_t cap) {
    if (cap == 0u) {
        return 0u;
    }
    char tmp[32];
    size_t idx = 0u;
    do {
        tmp[idx++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v != 0u && idx < sizeof(tmp));

    size_t pos = 0u;
    while (idx > 0u && pos + 1u < cap) {
        out[pos++] = tmp[--idx];
    }
    out[pos] = '\0';
    return pos;
}

static size_t f_to_str6(float v, char *out, size_t cap) {
    if (cap < 3u) {
        return 0u;
    }

    size_t pos = 0u;
    if (v < 0.0f) {
        out[pos++] = '-';
        v = -v;
        if (pos >= cap) {
            return pos - 1u;
        }
    }

    uint32_t scaled = (uint32_t)(v * 1000000.0f + 0.5f);
    uint32_t whole = scaled / 1000000u;
    uint32_t frac = scaled % 1000000u;

    pos += u32_to_str(whole, out + pos, cap - pos);
    if (pos + 7u >= cap) {
        return pos;
    }
    out[pos++] = '.';
    uint32_t div = 100000u;
    for (int i = 0; i < 6 && pos < cap; ++i) {
        out[pos++] = (char)('0' + (frac / div));
        frac %= div;
        div /= 10u;
    }
    if (pos < cap) {
        out[pos] = '\0';
    }
    return pos;
}

// Small append helpers for static buffers.
static size_t append_str(char *out, size_t pos, size_t cap, const char *src) {
    while (src && *src && pos + 1u < cap) {
        out[pos++] = *src++;
    }
    return pos;
}

static size_t append_char(char *out, size_t pos, size_t cap, char c) {
    if (pos + 1u < cap) {
        out[pos++] = c;
    }
    return pos;
}

static size_t append_u32(char *out, size_t pos, size_t cap, uint32_t v) {
    return pos + u32_to_str(v, out + pos, cap - pos);
}

static size_t append_u64(char *out, size_t pos, size_t cap, uint64_t v) {
    return pos + u64_to_str(v, out + pos, cap - pos);
}

static size_t append_f6(char *out, size_t pos, size_t cap, float v) {
    return pos + f_to_str6(v, out + pos, cap - pos);
}

// Lightweight float parser for positive/negative decimals.
static int parse_float(const char **p, float *out) {
    const char *s = *p;
    while (*s == ' ' || *s == '\t' || *s == ',') {
        ++s;
    }

    int neg = 0;
    if (*s == '+') {
        ++s;
    } else if (*s == '-') {
        neg = 1;
        ++s;
    }

    uint32_t whole = 0u;
    uint32_t frac = 0u;
    uint32_t frac_div = 1u;
    int has_digit = 0;

    while (*s >= '0' && *s <= '9') {
        has_digit = 1;
        whole = whole * 10u + (uint32_t)(*s - '0');
        ++s;
    }

    if (*s == '.') {
        ++s;
        while (*s >= '0' && *s <= '9') {
            has_digit = 1;
            frac = frac * 10u + (uint32_t)(*s - '0');
            frac_div *= 10u;
            ++s;
        }
    }

    if (!has_digit) {
        return 0;
    }

    float val = (float)whole + ((float)frac / (float)frac_div);
    if (neg) {
        val = -val;
    }
    *out = val;
    *p = s;
    return 1;
}

static void log_parse_error(int code) {
    char buf[48];
    size_t pos = 0u;
    pos = append_str(buf, pos, sizeof(buf), "[RDBG] parse_error code=");
    pos = append_u32(buf, pos, sizeof(buf), (uint32_t)code);
    pos = append_char(buf, pos, sizeof(buf), '\n');
    if (pos < sizeof(buf)) {
        write(STDERR_FILENO, buf, pos);
    }
}

// Compute a simple "energy" metric of the matrix.
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

// Trim whitespace and inline comments; returns new length.
static size_t t_inline(char *line) {
    if (!line) return 0u;

    // Strip newline / carriage return and trailing spaces.
    size_t len = strlen(line);
    while (len > 0u && (line[len - 1u] == '\n' || line[len - 1u] == '\r' ||
                        isspace((unsigned char)line[len - 1u]))) {
        line[--len] = '\0';
    }

    // Remove inline comments after '#'.
    char *h = strchr(line, '#');
    if (h) {
        *h = '\0';
        len = strlen(line);
        while (len > 0u && isspace((unsigned char)line[len - 1u])) {
            line[--len] = '\0';
        }
    }

    // Left trim.
    size_t off = 0u;
    while (line[off] != '\0' && isspace((unsigned char)line[off])) {
        ++off;
    }
    if (off > 0u) {
        memmove(line, line + off, len - off + 1u);
        len -= off;
    }

    return len;
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
    if (n < 2) {
        // At least timestamp and CPU are required.
    const char *p = line;
    float vals[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int count = 0;
    while (count < 4) {
        if (!parse_float(&p, &vals[count])) {
            break;
        }
        ++count;
        while (*p == ' ' || *p == '\t' || *p == ',' ) {
            ++p;
        }
        if (*p == '\0' || *p == '\n') {
            break;
        }
    }

    if (count < 2) {
        // At least CPU/mem required.
        return -3;
    }

    if (!isfinite(ts) || !isfinite(cpu)) {
        return -5; // reject NaN/inf inputs early
    }
    if (n < 3) {
        io = 0.0f; // default when missing
    } else if (!isfinite(io)) {
        return -5;
    }
    if (n < 4) {
        net = 0.0f; // default when missing
    } else if (!isfinite(net)) {
        return -5;
    }

    if (!f->has_t0) {
        f->t0 = ts;
        f->has_t0 = 1;
    }

    float rel = ts - f->t0;
    if (rel < 0.0f) {
        rel = 0.0f; // clamp backwards drift
    }

    // Map timestamp to the fixed delta grid (floor to keep deterministic bins).
    uint32_t idx = (uint32_t)floorf(rel / R_TS_DELTA + 1e-6f);
    if (idx >= R_MAX_ROWS) {
        return -4; // out of buffer
    }
    // Normalize to [0,1] in a simple way; real thresholds should be configurable.
    f->m[r][0] = n01(vals[0], 0.0f, 100.0f);     // CPU %
    f->m[r][1] = n01(vals[1], 0.0f, 4096.0f);    // Memory MB
    f->m[r][2] = n01(vals[2], 0.0f, 1000.0f);    // I/O ops
    f->m[r][3] = n01(vals[3], 0.0f, 4096.0f);    // Thread count

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
        "{\"tag\":\"%s\",\"severity\":%u,\"rows\":%u,\"cols\":%u,\"crc\":%u,\"epoch\":%llu,\"stats\":[%.6f,%.6f,%.6f,%.6f]}",
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
static void j(const RField *f, char *out, size_t out_len, const char *tag) {
    if (!out || out_len == 0u) return;

    if (!tag) tag = "userland_debug";

    size_t pos = 0u;
    pos = append_char(out, pos, out_len, '{');
    pos = append_str(out, pos, out_len, "\"tag\":\"");
    pos = append_str(out, pos, out_len, tag);
    pos = append_str(out, pos, out_len, "\",");

    pos = append_str(out, pos, out_len, "\"severity\":");
    pos = append_u32(out, pos, out_len, f->severity);
    pos = append_char(out, pos, out_len, ',');

    pos = append_str(out, pos, out_len, "\"rows\":");
    pos = append_u32(out, pos, out_len, f->r_used);
    pos = append_char(out, pos, out_len, ',');

    pos = append_str(out, pos, out_len, "\"cols\":");
    pos = append_u32(out, pos, out_len, f->c_used);
    pos = append_char(out, pos, out_len, ',');

    pos = append_str(out, pos, out_len, "\"crc\":");
    pos = append_u32(out, pos, out_len, f->crc_state);
    pos = append_char(out, pos, out_len, ',');

    pos = append_str(out, pos, out_len, "\"epoch\":");
    pos = append_u64(out, pos, out_len, f->t_epoch);
    pos = append_char(out, pos, out_len, ',');

    pos = append_str(out, pos, out_len, "\"matrix\":[");

    for (uint32_t i = 0u; i < f->r_used && pos < out_len; ++i) {
        pos = append_char(out, pos, out_len, '[');
        for (uint32_t j = 0u; j < f->c_used && pos < out_len; ++j) {
            pos = append_f6(out, pos, out_len, f->m[i][j]);
            if (j + 1u < f->c_used) {
                pos = append_char(out, pos, out_len, ',');
            }
        }
        pos = append_char(out, pos, out_len, ']');
        if (i + 1u < f->r_used) {
            pos = append_char(out, pos, out_len, ',');
        }
    }

    pos = append_char(out, pos, out_len, ']');
    pos = append_char(out, pos, out_len, '}');

    if (pos < out_len) {
        out[pos] = '\0';
    } else if (out_len > 0u) {
        out[out_len - 1u] = '\0';
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
    f.c_used = R_STATE_COLS; // Always report full state width

    // Time snapshot for traceability (ISO 27001 logging).
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        f.t_epoch = (uint64_t)ts.tv_sec;
    }

    // Strict stdin processing: CSV lines from shell or another tool.
    char line[256];
    while (f.r_used < R_MAX_ROWS && fgets(line, sizeof(line), stdin)) {
        if (t_inline(line) == 0u) {
            continue; // ignore comments / empty after trimming
    size_t line_len = 0u;
    char in_buf[512];

    while (f.r_used < R_MAX_ROWS) {
        ssize_t got = read(STDIN_FILENO, in_buf, sizeof(in_buf));
        if (got <= 0) {
            break;
        }
        for (ssize_t i = 0; i < got && f.r_used < R_MAX_ROWS; ++i) {
            char c = in_buf[i];
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                line[line_len] = '\0';
                if (line_len > 0u && line[0] != '#') {
                    int r = v(line, &f);
                    if (r != 0) {
                        log_parse_error(r);
                    }
                }
                line_len = 0u;
                continue;
            }
            if (line_len + 1u < sizeof(line)) {
                line[line_len++] = c;
            }
        }
    }

    if (line_len > 0u && f.r_used < R_MAX_ROWS) {
        line[line_len] = '\0';
        if (line[0] != '#') {
            int r = v(line, &f);
            if (r != 0) {
                log_parse_error(r);
            }
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
    // Always one single JSON object to stdout: safe for pipes.
    size_t out_len = strlen(json);
    if (out_len > 0u) {
        write(STDOUT_FILENO, json, out_len);
    }
    write(STDOUT_FILENO, "\n", 1u);

    return 0;
}
