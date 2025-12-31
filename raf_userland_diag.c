// raf_userland_diag.c
// Draft – UserLAnd developer-mode tracer (low-level, C11)
// Focus: matrix diffing for debugging signals; no dynamic allocation.
// Function blocks include defensive notes to surface mitigations for common bugs.

#include "core/include/raf_core.h"
#include <stdint.h>

#define RAF_STDIN_FD  0
#define RAF_STDOUT_FD 1
#define RAF_STDERR_FD 2

typedef long raf_ssize_t;

__attribute__((weak)) raf_ssize_t raf_read(int fd, void *buf, unsigned long count) {
    (void)fd;
    (void)buf;
    (void)count;
    return -1;
}

__attribute__((weak)) raf_ssize_t raf_write(int fd, const void *buf, unsigned long count) {
    (void)fd;
    (void)buf;
    (void)count;
    return -1;
}

__attribute__((weak)) uint64_t raf_epoch_seconds(void) {
    return 0u;
}

#define Q_ROWS 48u
#define Q_COLS 8u
#define Q_JSON 4096u

typedef struct {
    float a[Q_ROWS][Q_COLS];   // Baseline matrix
    float b[Q_ROWS][Q_COLS];   // Live matrix
    uint32_t ra;               // Rows in baseline
    uint32_t rb;               // Rows in live
    uint32_t c;                // Column count used
    uint32_t seen_a;           // Baseline column mask (interop guard)
    uint32_t seen_b;           // Live column mask (interop guard)
    uint32_t phase;            // 0=baseline,1=live
    uint32_t crc_a;            // CRC baseline
    uint32_t crc_b;            // CRC live
    uint32_t sev;              // Severity flag
    uint32_t bad;              // Input anomaly counter
    float drift;               // Mean absolute delta
    float peak;                // Maximum delta
    float spread;              // Drift-to-peak ratio for stability tuning
    uint64_t epoch;            // Timestamp (for audit)
} QTrace;

// Clamp-normalize into [0,1] to keep matrices stable.
// static to avoid symbol leakage; branchless bounds keep footprint low.
static float n01(float v, float min, float max) {
    if (max <= min) return 0.0f;
    float x = (v - min) / (max - min);
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    return x;
}

// Initialize structure with secure defaults.
// Sets deterministic baseline to avoid uninitialized data across phases.
static void t(QTrace *q) {
    uint8_t *ptr = (uint8_t *)q;
    for (size_t i = 0u; i < sizeof(*q); ++i) {
        ptr[i] = 0u;
    }
    q->c = 4u;            // default four metrics
    q->epoch = raf_epoch_seconds();
}

// Lightweight finite check to avoid pulling math headers; favors low-level guards.
static int f_ok(float v) {
    if (v != v) return 0;                    // NaN guard
    if (v > 3.4e38f || v < -3.4e38f) return 0; // crude INF guard
    return 1;
}

// Normalize with anomaly counting; keeps branch count minimal and footprint low.
static float c01(float v, float min, float max, uint32_t *bad) {
    if (!f_ok(v)) {
        if (bad) { ++(*bad); }
        v = 0.0f;
    }
    if (v < min || v > max) {
        if (bad) { ++(*bad); }
    }
    return n01(v, min, max);
}

// Locale-independent float parser (comma-separated, dot decimal).
static int parse_float(const char **cursor, float *out) {
    const char *s = *cursor;
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
    *cursor = s;
    return 1;
}

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

static void log_parse_error(int code) {
    char buf[64];
    size_t pos = 0u;
    pos = append_str(buf, pos, sizeof(buf), "[RTRACE] parse_error code=");
    pos = append_u32(buf, pos, sizeof(buf), (uint32_t)code);
    pos = append_char(buf, pos, sizeof(buf), '\n');
    if (pos < sizeof(buf)) {
        raf_write(RAF_STDERR_FD, buf, (unsigned long)pos);
    }
}

// Parse a CSV line into either baseline or live matrix.
// Manual parsing avoids locale surprises and truncates at Q_COLS to cap footprint.
static int u(QTrace *q, const char *line) {
    if (!q || !line) return -1;

    float vals[Q_COLS];
    uint32_t count = 0u;
    const char *p = line;

    while (*p != '\0' && count < Q_COLS) {
        while (*p == ' ' || *p == '\t') { ++p; }
        if (*p == '#') break;            // inline comment guard
        if (*p == '\0' || *p == '\n') break;

        if (!parse_float(&p, &vals[count])) {
            break;
        }
        ++count;

        while (*p == ' ' || *p == '\t' || *p == ',') { ++p; }
    }

    if (count == 0u) return -2;
    if (count > 4u) {
        count = 4u;
    }

    // Normalize and store; caps guard against overrun and mismatched column sets.
    float cpu = vals[0];
    float mem = (count > 1u) ? vals[1] : 0.0f;
    float io  = (count > 2u) ? vals[2] : 0.0f;
    float thr = (count > 3u) ? vals[3] : 0.0f;

    if (q->phase == 0u) {
        if (q->ra >= Q_ROWS) return -3;
        uint32_t r = q->ra++;
        q->a[r][0] = c01(cpu, 0.0f, 100.0f, &q->bad);
        q->a[r][1] = c01(mem, 0.0f, 4096.0f, &q->bad);
        q->a[r][2] = c01(io,  0.0f, 2000.0f, &q->bad);
        q->a[r][3] = c01(thr, 0.0f, 4096.0f, &q->bad);
        for (uint32_t j = 0u; j < count; ++j) {
            q->seen_a |= (1u << j);
        }
    } else {
        if (q->rb >= Q_ROWS) return -4;
        uint32_t r = q->rb++;
        q->b[r][0] = c01(cpu, 0.0f, 100.0f, &q->bad);
        q->b[r][1] = c01(mem, 0.0f, 4096.0f, &q->bad);
        q->b[r][2] = c01(io,  0.0f, 2000.0f, &q->bad);
        q->b[r][3] = c01(thr, 0.0f, 4096.0f, &q->bad);
        for (uint32_t j = 0u; j < count; ++j) {
            q->seen_b |= (1u << j);
        }
    }

    // Retain highest column count seen to signal interoperability coverage.
    if (count > q->c) {
        q->c = count;
    }

    return 0;
}

// Compute drift metrics and severity.
// Uses absolute deltas to keep monotonic severity independent of sign.
static void y(QTrace *q) {
    uint32_t rows = q->ra < q->rb ? q->ra : q->rb;
    float acc = 0.0f;
    float peak = 0.0f;

    for (uint32_t i = 0; i < rows; ++i) {
        for (uint32_t j = 0; j < q->c; ++j) {
            float d = q->a[i][j] - q->b[i][j];
            if (d < 0.0f) d = -d;
            acc += d;
            if (d > peak) peak = d;
        }
    }

    float total = (float)(rows * q->c);
    q->drift = (total > 0.0f) ? (acc / total) : 0.0f;
    q->peak = peak;
    q->spread = (peak > 0.0f) ? (q->drift / peak) : 0.0f;

    uint32_t missing = (q->ra > q->rb) ? (q->ra - q->rb) : (q->rb - q->ra);
    uint32_t mask_gap = q->seen_a ^ q->seen_b; // column availability gap
    // Severity mapping: strict to favor stability. Missing rows bump severity one step.
    if (q->peak > 0.75f || q->drift > 0.45f) {
        q->sev = 2u; // critical
    } else if (q->peak > 0.35f || q->drift > 0.20f) {
        q->sev = 1u; // warning
    } else {
        q->sev = 0u; // nominal
    }
    if ((missing > 0u || q->bad > 0u || mask_gap != 0u) && q->sev < 2u) {
        ++q->sev;
    }

    q->crc_a = raf_fast_crc32(0u, q->a, sizeof(float) * q->ra * q->c);
    q->crc_b = raf_fast_crc32(0u, q->b, sizeof(float) * q->rb * q->c);
}

// Emit JSON payload for external consumption.
// Ensures bounded buffer writes and falls back to minimal payload on overflow.
static void o(const QTrace *q, char *out, size_t out_len) {
    if (!out || out_len == 0u) return;
    uint32_t mask_gap = q->seen_a ^ q->seen_b;
    size_t pos = 0u;
    pos = append_char(out, pos, out_len, '{');
    pos = append_str(out, pos, out_len, "\"tag\":\"userland_trace\",");
    pos = append_str(out, pos, out_len, "\"severity\":");
    pos = append_u32(out, pos, out_len, q->sev);
    pos = append_char(out, pos, out_len, ',');
    pos = append_str(out, pos, out_len, "\"rows_a\":");
    pos = append_u32(out, pos, out_len, q->ra);
    pos = append_char(out, pos, out_len, ',');
    pos = append_str(out, pos, out_len, "\"rows_b\":");
    pos = append_u32(out, pos, out_len, q->rb);
    pos = append_char(out, pos, out_len, ',');
    pos = append_str(out, pos, out_len, "\"cols\":");
    pos = append_u32(out, pos, out_len, q->c);
    pos = append_char(out, pos, out_len, ',');
    pos = append_str(out, pos, out_len, "\"crc_a\":");
    pos = append_u32(out, pos, out_len, q->crc_a);
    pos = append_char(out, pos, out_len, ',');
    pos = append_str(out, pos, out_len, "\"crc_b\":");
    pos = append_u32(out, pos, out_len, q->crc_b);
    pos = append_char(out, pos, out_len, ',');
    pos = append_str(out, pos, out_len, "\"bad\":");
    pos = append_u32(out, pos, out_len, q->bad);
    pos = append_char(out, pos, out_len, ',');
    pos = append_str(out, pos, out_len, "\"col_gap\":");
    pos = append_u32(out, pos, out_len, mask_gap);
    pos = append_char(out, pos, out_len, ',');
    pos = append_str(out, pos, out_len, "\"drift\":");
    pos = append_f6(out, pos, out_len, q->drift);
    pos = append_char(out, pos, out_len, ',');
    pos = append_str(out, pos, out_len, "\"peak\":");
    pos = append_f6(out, pos, out_len, q->peak);
    pos = append_char(out, pos, out_len, ',');
    pos = append_str(out, pos, out_len, "\"spread\":");
    pos = append_f6(out, pos, out_len, q->spread);
    pos = append_char(out, pos, out_len, ',');
    pos = append_str(out, pos, out_len, "\"epoch\":");
    pos = append_u64(out, pos, out_len, q->epoch);
    pos = append_str(out, pos, out_len, ",\"note\":\"baseline_then_live_with_blank_line\"}");

    if (pos < out_len) {
        out[pos] = '\0';
    } else if (out_len > 0u) {
        out[out_len - 1u] = '\0';
    }
}

int main(void) {
    QTrace q;
    t(&q);

    char line[256];
    size_t line_len = 0u;
    int line_overflow = 0;
    char in_buf[512];
    ssize_t got = 0;
    while ((got = raf_read(RAF_STDIN_FD, in_buf, sizeof(in_buf))) > 0) {
        for (raf_ssize_t i = 0; i < got; ++i) {
            char c = in_buf[i];
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                line[line_len] = '\0';
                if (line_overflow) {
                    log_parse_error(-6);
                } else if (line_len == 0u) {
                    q.phase = 1u;
                } else if (line[0] != '#') {
                    int r = u(&q, line);
                    if (r != 0) {
                        log_parse_error(r);
                    }
                }
                line_len = 0u;
                line_overflow = 0;
                continue;
            }
            if (!line_overflow && line_len + 1u < sizeof(line)) {
                line[line_len++] = c;
            } else {
                line_overflow = 1;
            }
        }
    }

    if (line_len > 0u) {
        line[line_len] = '\0';
        if (line_overflow) {
            log_parse_error(-6);
        } else if (line[0] != '#') {
            int r = u(&q, line);
            if (r != 0) {
                log_parse_error(r);
            }
        }
    }

    y(&q);

    char json[Q_JSON];
    o(&q, json, sizeof(json));
    size_t out_len = 0u;
    while (json[out_len] != '\0' && out_len < sizeof(json)) {
        ++out_len;
    }
    if (out_len > 0u) {
        raf_write(RAF_STDOUT_FD, json, (unsigned long)out_len);
    }
    raf_write(RAF_STDOUT_FD, "\n", 1u);

    return 0;
}
