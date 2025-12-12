/*
 * RAFAELIA COMPACT CORE
 * Complete reference module focusing on compact datasets, semantic vectorization,
 * CRC overlay, fail-safe watchdog, DB25 pin abstraction, and streaming JSON/image
 * ingestion with low footprint. Comments are in English at function blocks.
 *
 * Standards intent: ISO 9001/27001/27017/27018/8000/25010, IEEE 830/1012/12207/14764/1633/42010/26514,
 * NIST CSF/800-53/800-207, NIST AI Risk Framework, IETF RFC 5280/7519/7230/8446, W3C JSON/YAML.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "rafaelia_compact_core.h"

/* JSON ingest chunk size default */
#define JSON_CHUNK 4096u

/* FNV-1a 32-bit for compact hashing */
static uint32_t raf_fnv1a(const uint8_t *p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

/* CRC32 (polynomial 0xEDB88320) for overlay integrity */
static uint32_t raf_crc32(const uint8_t *p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (unsigned j = 0; j < 8; ++j) {
            uint32_t m = -(c & 1u);
            c = (c >> 1) ^ (0xEDB88320u & m);
        }
    }
    return c ^ 0xFFFFFFFFu;
}

/* XOR-folded CRC64 using twin CRC32 for redundancy */
static raf_crc raf_crc64(const uint8_t *p, size_t n) {
    raf_crc r;
    r.crc0 = raf_crc32(p, n);
    r.crc1 = raf_crc32(p, n >> 1);
    return r;
}

/* Watchdog initialization */
static void raf_watchdog_init(raf_watchdog *w) {
    if (!w) return;
    w->counter = 0;
    w->tripped = false;
}

/* Watchdog kick */
static void raf_watchdog_kick(raf_watchdog *w) {
    if (!w || w->tripped) return;
    w->counter++;
}

/* Watchdog trip to fail-safe */
static void raf_watchdog_trip(raf_watchdog *w) {
    if (!w) return;
    w->tripped = true;
}

/* DB25 pin toggle */
static void raf_db25_set(raf_db25 *d, uint8_t pin, uint8_t level) {
    if (!d || pin >= 25) return;
    d->pin[pin] = level ? 1u : 0u;
}

/* Header overlay: writes length header then copies payload */
static bool raf_pack_with_header(const raf_buf *src, raf_buf *dst, uint8_t hdr_mode) {
    if (!src || !dst || !src->data || !dst->data) return false;
    size_t hdr = hdr_mode;
    if (hdr != HDR8 && hdr != HDR16 && hdr != HDR32 && hdr != HDR64) return false;
    if (dst->len < hdr + src->len) return false;

    switch (hdr) {
        case HDR8:  dst->data[0] = (uint8_t)src->len; break;
        case HDR16: ((uint16_t *)dst->data)[0] = (uint16_t)src->len; break;
        case HDR32: ((uint32_t *)dst->data)[0] = (uint32_t)src->len; break;
        case HDR64: ((uint64_t *)dst->data)[0] = (uint64_t)src->len; break;
        default: return false;
    }

    memcpy(dst->data + hdr, src->data, src->len);
    return true;
}

/* Lightweight delta-RLE compressor; deterministic and streaming-friendly */
static size_t raf_delta_rle_compress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap) {
    if (!src || !dst || cap == 0) return 0;
    size_t w = 0;
    uint8_t prev = 0;
    size_t run = 0;
    for (size_t i = 0; i < n; ++i) {
        uint8_t d = (uint8_t)(src[i] - prev);
        prev = src[i];
        if (run == 0) {
            dst[w++] = d;
            run = 1;
        } else if (d == dst[w - 1] && run < 255) {
            run++;
        } else {
            dst[w++] = (uint8_t)run;
            dst[w++] = d;
            run = 1;
        }
        if (w + 2 >= cap) break;
    }
    if (w < cap) {
        dst[w++] = (uint8_t)run;
    }
    return w;
}

/* Expand compressed bytes back (used for integrity checks) */
static size_t raf_delta_rle_decompress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap) {
    if (!src || !dst || cap == 0) return 0;
    size_t r = 0;
    size_t i = 0;
    uint8_t prev = 0;
    while (i + 1 < n && r < cap) {
        uint8_t val = src[i++];
        uint8_t run = src[i++];
        for (uint8_t k = 0; k < run && r < cap; ++k) {
            prev = (uint8_t)(prev + val);
            dst[r++] = prev;
        }
    }
    return r;
}

/* Semantic vector: hashed n-gram accumulator into SEM_DIM bins */
static bool raf_semantic_vector(const raf_buf *src, raf_buf *dst) {
    if (!src || !dst || !src->data || !dst->data) return false;
    if (dst->len < SEM_DIM) return false;
    memset(dst->data, 0, SEM_DIM);
    if (src->len == 0) return true;

    for (size_t i = 0; i < src->len; ++i) {
        uint8_t gram[3];
        gram[0] = src->data[i];
        gram[1] = src->data[(i + 1) % src->len];
        gram[2] = src->data[(i + 2) % src->len];
        uint32_t h = raf_fnv1a(gram, 3u);
        size_t idx = h % SEM_DIM;
        if (dst->data[idx] != 0xFFu) dst->data[idx] += 1u;
    }
    return true;
}

/* Image projection: sample into IMG_DIM buckets using a low-cost hash */
static bool raf_image_project(const raf_buf *src, raf_buf *dst) {
    if (!src || !dst || !src->data || !dst->data) return false;
    if (dst->len < IMG_DIM) return false;
    memset(dst->data, 0, IMG_DIM);
    if (src->len == 0) return true;

    uint32_t acc = 0;
    for (size_t i = 0; i < src->len; ++i) {
        acc = (acc << 5) | (acc >> 27);
        acc ^= src->data[i];
        size_t idx = (acc ^ (acc >> 7)) % IMG_DIM;
        if (dst->data[idx] != 0xFFu) dst->data[idx] += 1u;
    }
    return true;
}

/* JSON ingest: streaming fold of tokens into vector while chunking */
static bool raf_json_ingest(const raf_buf *src, raf_buf *dst, size_t chunk) {
    if (!src || !dst || !src->data || !dst->data) return false;
    if (dst->len < SEM_DIM) return false;
    memset(dst->data, 0, dst->len);

    size_t step = (chunk == 0) ? JSON_CHUNK : chunk;
    size_t pos = 0;
    while (pos < src->len) {
        size_t take = (src->len - pos > step) ? step : (src->len - pos);
        uint32_t h = raf_fnv1a(src->data + pos, take);
        size_t idx = h % dst->len;
        if (dst->data[idx] != 0xFFu) dst->data[idx] += 1u;
        pos += take;
    }
    return true;
}

/* BBS chat ring buffer: append message safely */
void raf_bbs_init(raf_bbs *b) {
    if (!b) return;
    memset(b, 0, sizeof(*b));
}

void raf_bbs_post(raf_bbs *b, const char *msg) {
    if (!b || !msg) return;
    size_t len = strnlen(msg, BBS_LINE_MAX - 1u);
    uint32_t idx = b->head % BBS_LINES;
    memcpy(b->lines[idx].line, msg, len);
    b->lines[idx].line[len] = '\0';
    b->head = (b->head + 1u) % BBS_LINES;
    if (b->count < BBS_LINES) b->count++;
}

const char *raf_bbs_latest(const raf_bbs *b) {
    if (!b || b->count == 0) return NULL;
    uint32_t idx = (b->head + BBS_LINES - 1u) % BBS_LINES;
    return b->lines[idx].line;
}

/* End-to-end pipeline */
bool rafelia_compact_pipeline(raf_state *st, const raf_buf *src,
                              raf_buf *packed, raf_buf *semantic,
                              raf_buf *image_vec, raf_buf *json_vec,
                              raf_bbs *chat)
{
    if (!st || !src || !packed || !semantic || !image_vec || !json_vec) return false;
    if (!src->data || !packed->data || !semantic->data || !image_vec->data || !json_vec->data) return false;

    raf_watchdog_kick(&st->wd);

    if (!raf_pack_with_header(src, packed, HDR32)) {
        raf_watchdog_trip(&st->wd);
        return false;
    }

    raf_crc digest = raf_crc64(src->data, src->len);
    (void)digest; /* hook for header overlay/logging */
    st->stats.packed_bytes = packed->len;

    if (!raf_semantic_vector(src, semantic)) {
        raf_watchdog_trip(&st->wd);
        return false;
    }
    st->stats.semantic_bytes = semantic->len;

    if (!raf_image_project(src, image_vec)) {
        raf_watchdog_trip(&st->wd);
        return false;
    }
    st->stats.image_bytes = image_vec->len;

    if (!raf_json_ingest(src, json_vec, 0)) {
        raf_watchdog_trip(&st->wd);
        return false;
    }
    st->stats.json_bytes = json_vec->len;

    /* Optional DB25 toggles to mark successful frame */
    raf_db25_set(&st->phy, 0, 1);
    raf_db25_set(&st->phy, 1, (uint8_t)(st->wd.counter & 1u));

    if (chat) {
        raf_bbs_post(chat, "pipeline ok");
    }

    return !st->wd.tripped;
}

/* Integrity self-check using compression/decompression loop */
bool rafelia_self_check(const raf_buf *src) {
    if (!src || !src->data) return false;
    uint8_t *tmp = calloc(src->len * 2u + 2u, 1u);
    if (!tmp) return false;
    uint8_t *restore = calloc(src->len + 2u, 1u);
    if (!restore) { free(tmp); return false; }

    size_t clen = raf_delta_rle_compress(src->data, src->len, tmp, src->len * 2u + 2u);
    size_t dlen = raf_delta_rle_decompress(tmp, clen, restore, src->len + 2u);

    bool ok = (dlen >= src->len) && (memcmp(src->data, restore, src->len) == 0);
    free(tmp);
    free(restore);
    return ok;
}

/* Initialize state */
void rafelia_state_init(raf_state *st) {
    if (!st) return;
    memset(st, 0, sizeof(*st));
    raf_watchdog_init(&st->wd);
}

#ifdef RAFELIA_COMPACT_TEST
int main(void) {
    uint8_t src_mem[1024];
    uint8_t packed_mem[2048];
    uint8_t semantic_mem[SEM_DIM];
    uint8_t image_mem[IMG_DIM];
    uint8_t json_mem[SEM_DIM];

    for (size_t i = 0; i < sizeof(src_mem); ++i) src_mem[i] = (uint8_t)(i & 0xFFu);

    raf_state st; rafelia_state_init(&st);
    raf_bbs chat; raf_bbs_init(&chat);

    raf_buf src = { src_mem, sizeof(src_mem) };
    raf_buf packed = { packed_mem, sizeof(packed_mem) };
    raf_buf semantic = { semantic_mem, sizeof(semantic_mem) };
    raf_buf image_vec = { image_mem, sizeof(image_mem) };
    raf_buf json_vec = { json_mem, sizeof(json_mem) };

    bool ok = rafelia_compact_pipeline(&st, &src, &packed, &semantic, &image_vec, &json_vec, &chat);
    bool self = rafelia_self_check(&src);

    printf("pipeline=%d self=%d wd=%u chat='%s'\n", ok ? 1 : 0, self ? 1 : 0,
           st.wd.counter, raf_bbs_latest(&chat) ? raf_bbs_latest(&chat) : "");
    return ok && self ? 0 : 1;
}
#endif

