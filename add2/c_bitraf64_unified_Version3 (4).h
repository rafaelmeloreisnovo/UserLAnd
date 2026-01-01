#ifndef BITRAF64_UNIFIED_H
#define BITRAF64_UNIFIED_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bitraf64_ctx bitraf64_ctx;

// Create context: seed (uint64), selos UTF-8 string, verbose flag
bitraf64_ctx* bitraf64_create(uint64_t seed, const char* selos, int verbose);
void bitraf64_free(bitraf64_ctx* ctx);

// Encode: returns malloc'd buffer (header64 + payload). Caller must free. out_len receives total length.
uint8_t* bitraf64_encode(bitraf64_ctx* ctx, const uint8_t* input, size_t input_len, uint64_t block_index, size_t* out_len);

// Decode: returns malloc'd original payload. Caller must free. out_len receives payload length. NULL on error.
uint8_t* bitraf64_decode(bitraf64_ctx* ctx, const uint8_t* blob, size_t blob_len, size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif // BITRAF64_UNIFIED_H