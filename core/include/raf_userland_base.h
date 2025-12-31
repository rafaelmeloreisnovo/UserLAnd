#ifndef RAF_USERLAND_BASE_H
#define RAF_USERLAND_BASE_H

#if defined(__UINT8_TYPE__)
typedef __UINT8_TYPE__ uint8_t;
#else
typedef unsigned char uint8_t;
#endif

#if defined(__UINT32_TYPE__)
typedef __UINT32_TYPE__ uint32_t;
#else
typedef unsigned int uint32_t;
#endif

#if defined(__UINT64_TYPE__)
typedef __UINT64_TYPE__ uint64_t;
#else
typedef unsigned long long uint64_t;
#endif

#if defined(__INT32_TYPE__)
typedef __INT32_TYPE__ int32_t;
#else
typedef int int32_t;
#endif

#if defined(__SIZE_TYPE__)
typedef __SIZE_TYPE__ size_t;
#else
typedef unsigned long size_t;
#endif

static inline uint32_t raf_fast_crc32(uint32_t prev, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t c = ~prev;
    while (len--) {
        c ^= *p++;
        for (int k = 0; k < 8; ++k) {
            c = (c >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(c & 1u));
        }
    }
    return ~c;
}

#endif
