#ifndef RAFAELIA_CORE_MINIMAL_H
#define RAFAELIA_CORE_MINIMAL_H

/*
 * RAFAELIA MINIMAL CORE API
 * 
 * This is the minimal public API for the Rafaelia core library.
 * Only essential functions are exposed here. Implementation details
 * are kept in the impl/ directory.
 * 
 * PUBLIC API:
 * - CRC32 computation (used by JNI)
 * 
 * All other functionality is internal and should not be used directly
 * by external code.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Compute CRC32 checksum for data buffer.
 * 
 * @param data Pointer to data buffer (must not be NULL if len > 0)
 * @param len Length of data in bytes
 * @return CRC32 checksum value
 */
uint32_t raf_crc32(const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif  // RAFAELIA_CORE_MINIMAL_H
