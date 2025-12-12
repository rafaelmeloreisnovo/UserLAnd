#ifndef RAFAELIA_COMPACT_CORE_H
#define RAFAELIA_COMPACT_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define HDR8   1u
#define HDR16  2u
#define HDR32  4u
#define HDR64  8u
#define SEM_DIM 128u
#define IMG_DIM 64u
#define BBS_LINES 32u
#define BBS_LINE_MAX 160u

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *data;
    size_t len;
} raf_buf;

typedef struct {
    uint32_t crc0;
    uint32_t crc1;
} raf_crc;

typedef struct {
    uint32_t counter;
    bool tripped;
} raf_watchdog;

typedef struct {
    uint8_t pin[25];
} raf_db25;

typedef struct {
    size_t packed_bytes;
    size_t semantic_bytes;
    size_t image_bytes;
    size_t json_bytes;
} raf_stats;

typedef struct {
    raf_watchdog wd;
    raf_db25 phy;
    raf_stats stats;
} raf_state;

typedef struct {
    char line[BBS_LINE_MAX];
} raf_bbs_line;

typedef struct {
    raf_bbs_line lines[BBS_LINES];
    uint32_t head;
    uint32_t count;
} raf_bbs;

void rafelia_state_init(raf_state *st);
bool rafelia_compact_pipeline(raf_state *st, const raf_buf *src,
                              raf_buf *packed, raf_buf *semantic,
                              raf_buf *image_vec, raf_buf *json_vec,
                              raf_bbs *chat);
bool rafelia_self_check(const raf_buf *src);
void raf_bbs_init(raf_bbs *b);
void raf_bbs_post(raf_bbs *b, const char *msg);
const char *raf_bbs_latest(const raf_bbs *b);

#ifdef __cplusplus
}
#endif

#endif /* RAFAELIA_COMPACT_CORE_H */

