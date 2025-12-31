// raf_userland_diag_all.c
// Draft – Aggregated UserLAnd developer-mode tracer for multi-file coverage.
// Low-level, no dynamic allocation; function blocks include mitigation notes.

#include "raf_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define Q_ROWS   48u
#define Q_COLS   8u
#define Q_JSON   8192u
#define Q_GROUPS 12u

// Core trace container (baseline + live) with simple label tagging.
typedef struct {
    float a[Q_ROWS][Q_COLS];
    float b[Q_ROWS][Q_COLS];
    uint32_t ra;
    uint32_t rb;
    uint32_t c;
    uint32_t seen_a;
    uint32_t seen_b;
    uint32_t phase;
    uint32_t crc_a;
    uint32_t crc_b;
    uint32_t sev;
    uint32_t bad;
    float drift;
    float peak;
    float spread;
    uint64_t epoch;
    char tag[32];
    uint32_t used;
} QTrace;

// Clamp-normalize into [0,1]; branch guards avoid pulling heavy math headers.
static float n01(float v, float min, float max) {
    if (max <= min) return 0.0f;
    float x = (v - min) / (max - min);
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    return x;
}

// Finite guard to reject NaN/INF without external libs.
static int f_ok(float v) {
    if (v != v) return 0;                       // NaN
    if (v > 3.4e38f || v < -3.4e38f) return 0;  // crude INF
    return 1;
}

// Normalize with anomaly counting to expose malformed values upstream.
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

// Initialize trace container with deterministic defaults; label is trimmed.
static void t(QTrace *q, const char *tag) {
    memset(q, 0, sizeof(*q));
    q->c = 4u;
    q->epoch = (uint64_t)time(NULL);
    if (tag) {
        size_t len = strlen(tag);
        if (len >= sizeof(q->tag)) {
            len = sizeof(q->tag) - 1u;
        }
        memcpy(q->tag, tag, len);
        q->tag[len] = '\0';
    } else {
        q->tag[0] = '\0';
    }
    q->used = 1u;
}

// Parse CSV row into baseline or live matrices; truncates to Q_COLS for safety.
static int u(QTrace *q, const char *line) {
    if (!q || !line || q->used == 0u) return -1;

    float vals[Q_COLS];
    uint32_t count = 0u;
    const char *p = line;

    while (*p != '\0' && count < Q_COLS) {
        while (*p == ' ' || *p == '\t') { ++p; }
        if (*p == '#') break;
        if (*p == '\0' || *p == '\n') break;

        char *endptr = NULL;
        vals[count] = strtof(p, &endptr);
        if (endptr == p) break;
        ++count;

        while (*endptr == ' ' || *endptr == '\t') { ++endptr; }
        if (*endptr == ',') { ++endptr; }
        p = endptr;
    }

    if (count == 0u) return -2;
    if (count > 4u) {
        count = 4u;
    }

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
    } else if (q->phase == 1u) {
        if (q->rb >= Q_ROWS) return -4;
        uint32_t r = q->rb++;
        q->b[r][0] = c01(cpu, 0.0f, 100.0f, &q->bad);
        q->b[r][1] = c01(mem, 0.0f, 4096.0f, &q->bad);
        q->b[r][2] = c01(io,  0.0f, 2000.0f, &q->bad);
        q->b[r][3] = c01(thr, 0.0f, 4096.0f, &q->bad);
        for (uint32_t j = 0u; j < count; ++j) {
            q->seen_b |= (1u << j);
        }
    } else {
        return -5; // Already sealed; requires new trace header.
    }

    if (count > q->c) {
        q->c = count;
    }

    return 0;
}

// Compute drift and severity; uses absolute deltas for monotonic scoring.
static void y(QTrace *q) {
    uint32_t rows = (q->ra < q->rb) ? q->ra : q->rb;
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
    uint32_t mask_gap = q->seen_a ^ q->seen_b;
    if (q->peak > 0.75f || q->drift > 0.45f) {
        q->sev = 2u;
    } else if (q->peak > 0.35f || q->drift > 0.20f) {
        q->sev = 1u;
    } else {
        q->sev = 0u;
    }
    if ((missing > 0u || q->bad > 0u || mask_gap != 0u) && q->sev < 2u) {
        ++q->sev;
    }

    q->crc_a = raf_fast_crc32(0u, q->a, sizeof(float) * q->ra * q->c);
    q->crc_b = raf_fast_crc32(0u, q->b, sizeof(float) * q->rb * q->c);
}

// JSON string escape helper; writes escaped content with NUL termination.
static size_t json_escape(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0u) return 0u;
    size_t pos = 0u;
    dst[0] = '\0';
    if (!src) return 0u;

    while (*src != '\0' && pos + 1u < cap) {
        unsigned char c = (unsigned char)*src++;
        const char *esc = NULL;

        switch (c) {
            case '\"': esc = "\\\""; break;
            case '\\': esc = "\\\\"; break;
            case '\n': esc = "\\n"; break;
            case '\r': esc = "\\r"; break;
            case '\t': esc = "\\t"; break;
            default:
                if (c < 0x20u) {
                    if (pos + 7u > cap) {
                        break;
                    }
                    int w = snprintf(dst + pos, cap - pos, "\\u%04x", c);
                    if (w > 0) {
                        pos += (size_t)w;
                    }
                    continue;
                }
                dst[pos++] = (char)c;
                continue;
        }

        if (!esc) {
            break;
        }
        size_t esc_len = 2u;  // All escape sequences above are exactly two characters long.
        if (pos + esc_len + 1u > cap) {
            break;
        }
        memcpy(dst + pos, esc, esc_len);
        pos += esc_len;
    }

    dst[pos] = '\0';
    return pos;
}

// Append JSON snippet into buffer; keeps bounded writes to avoid overflow.
static size_t o(char *dst, size_t cap, const QTrace *q) {
    if (!dst || cap == 0u || !q) return 0u;
    uint32_t mask_gap = q->seen_a ^ q->seen_b;
    char tag_esc[192];
    json_escape(tag_esc, sizeof(tag_esc), q->tag);
    int w = snprintf(dst, cap,
        "{\"tag\":\"%s\","\
        "\"severity\":%u,"\
        "\"rows_a\":%u,"\
        "\"rows_b\":%u,"\
        "\"cols\":%u,"\
        "\"crc_a\":%u,"\
        "\"crc_b\":%u,"\
        "\"bad\":%u,"\
        "\"col_gap\":%u,"\
        "\"drift\":%.6f,"\
        "\"peak\":%.6f,"\
        "\"spread\":%.6f,"\
        "\"epoch\":%llu}" ,
        tag_esc,
        q->sev,
        q->ra,
        q->rb,
        q->c,
        q->crc_a,
        q->crc_b,
        q->bad,
        mask_gap,
        (double)q->drift,
        (double)q->peak,
        (double)q->spread,
        (unsigned long long)q->epoch
    );

    if (w < 0) return 0u;
    if ((size_t)w >= cap) {
        if (cap > 2u) {
            dst[0] = '{'; dst[1] = '}'; dst[2] = '\0';
            return 2u;
        }
        return 0u;
    }
    return (size_t)w;
}

// Emit aggregated JSON array; truncated safely if exceeding Q_JSON.
static void oa(const QTrace *q, uint32_t count, char *out, size_t out_len) {
    if (!out || out_len == 0u) return;
    size_t pos = 0u;
    if (out_len > 1u) {
        out[pos++] = '[';
    }

    for (uint32_t i = 0; i < count; ++i) {
        if (q[i].used == 0u) continue;
        if (pos + 1u >= out_len) break;
        if (pos > 1u) {
            out[pos++] = ',';
        }
        size_t written = o(out + pos, out_len - pos, &q[i]);
        pos += written;
        if (pos >= out_len) {
            pos = out_len - 1u;
            break;
        }
    }

    if (pos + 1u < out_len) {
        out[pos++] = ']';
        out[pos] = '\0';
    } else if (out_len > 0u) {
        out[out_len - 1u] = '\0';
    }
}

// Simple parser for multi-trace streams:
// - Lines beginning with '@' start a new trace and carry the tag.
// - Blank line toggles baseline→live, second blank seals the trace.
// - Data rows are CSV metrics (cpu,mem,io,thr) per line.
int main(void) {
    QTrace q[Q_GROUPS];
    memset(q, 0, sizeof(q));

    uint32_t idx = Q_GROUPS; // sentinel for "no active trace"
    uint32_t count = 0u;
    char line[256];
    uint32_t blank_run = 0u;

    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == '#') continue;

        if (line[0] == '@') {
            blank_run = 0u;
            if (count < Q_GROUPS) {
                idx = count++;
                const char *name = line + 1;
                while (*name == ' ' || *name == '\t') { ++name; }
                size_t len = strcspn(name, "\r\n");
                char tag[32];
                if (len >= sizeof(tag)) len = sizeof(tag) - 1u;
                memcpy(tag, name, len);
                tag[len] = '\0';
                t(&q[idx], tag);
            } else {
                idx = Q_GROUPS; // ignore extra traces to keep bounds safe
            }
            continue;
        }

        if (line[0] == '\n' || line[0] == '\r') {
            ++blank_run;
            if (idx < Q_GROUPS && q[idx].used != 0u) {
                if (q[idx].phase == 0u) {
                    q[idx].phase = 1u;
                } else if (blank_run > 1u) {
                    q[idx].phase = 2u; // sealed
                    idx = Q_GROUPS;
                }
            }
            continue;
        }

        blank_run = 0u;
        if (idx < Q_GROUPS && q[idx].used != 0u) {
            int r = u(&q[idx], line);
            if (r != 0) {
                fprintf(stderr, "[RTRACE_ALL] parse_error idx=%u code=%d\n", idx, r);
            }
        }
    }

    for (uint32_t i = 0; i < count; ++i) {
        y(&q[i]);
    }

    char json[Q_JSON];
    oa(q, count, json, sizeof(json));
    fputs(json, stdout);
    fputc('\n', stdout);
    return 0;
}
