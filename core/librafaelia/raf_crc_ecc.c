#include <stdint.h>
#include <stddef.h>

#include "raf_core_minimal.h"
#include "raf_core.h"

uint32_t raf_crc32(const uint8_t *data, uint32_t len) {
    if (data == NULL || len == 0) {
        return 0u;
    }
    return raf_fast_crc32(0u, data, (size_t)len);
}
