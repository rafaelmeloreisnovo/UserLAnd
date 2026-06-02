#include "raf_compile_key.h"

static uint32_t raf_ck_crc32_step(uint32_t crc, uint8_t byte) {
    crc ^= (uint32_t)byte;
    for (uint32_t bit = 0u; bit < 8u; bit++) {
        uint32_t mask = 0u - (crc & 1u);
        crc = (crc >> 1u) ^ (0xEDB88320u & mask);
    }
    return crc;
}

uint32_t raf_compile_key_crc32(const uint8_t *data, uint32_t len) {
    if (data == 0 || len == 0u) return 0u;

    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0u; i < len; i++) crc = raf_ck_crc32_step(crc, data[i]);
    return ~crc;
}

uint16_t raf_compile_key_entropy_milli(const uint8_t *data, uint32_t len) {
    if (data == 0 || len == 0u) return 0u;

    uint8_t seen[256];
    for (uint32_t i = 0u; i < 256u; i++) seen[i] = 0u;

    uint32_t unique = 0u;
    uint32_t transitions = 0u;
    uint8_t prev = data[0];

    for (uint32_t i = 0u; i < len; i++) {
        uint8_t byte = data[i];
        unique += (seen[byte] == 0u) ? 1u : 0u;
        seen[byte] = 1u;
        if (i != 0u) transitions += (byte != prev) ? 1u : 0u;
        prev = byte;
    }

    uint32_t entropy = (unique * 6000u) / 256u;
    if (len > 1u) entropy += (transitions * 2000u) / (len - 1u);
    return (uint16_t)(entropy > 8000u ? 8000u : entropy);
}

uint16_t raf_compile_key_tag14(const uint8_t *data, uint32_t len) {
    if (data == 0 || len == 0u) return 0u;

    uint32_t tag = 0x2A5Au;
    for (uint32_t i = 0u; i < len; i++) {
        tag ^= ((uint32_t)data[i] + (i * 0x45D9u));
        tag = ((tag << 5u) | (tag >> 9u)) & 0x3FFFu;
        tag ^= (tag >> 7u);
        tag &= 0x3FFFu;
    }
    return (uint16_t)tag;
}

RafCompileKeyReport raf_compile_key_validate(const uint8_t *data,
                                              uint32_t len,
                                              uint32_t flags,
                                              uint32_t expected_crc32) {
    RafCompileKeyReport report;
    report.status = RAF_COMPILE_STATUS_OK;
    report.crc32 = 0u;
    report.entropy_milli = 0u;
    report.tag14 = 0u;

    if (data == 0) report.status |= RAF_COMPILE_STATUS_NULL_INPUT;
    if (len < RAF_COMPILE_KEY_MIN_LEN) report.status |= RAF_COMPILE_STATUS_LENGTH_SHORT;
    if (len > RAF_COMPILE_KEY_MAX_LEN) report.status |= RAF_COMPILE_STATUS_LENGTH_LONG;
    if ((flags & RAF_COMPILE_FLAGS_REQUIRED) != RAF_COMPILE_FLAGS_REQUIRED) {
        report.status |= RAF_COMPILE_STATUS_MISSING_LICENSE_FLAGS;
    }
    if (data == 0 || len == 0u) return report;

    report.crc32 = raf_compile_key_crc32(data, len);
    report.entropy_milli = raf_compile_key_entropy_milli(data, len);
    report.tag14 = raf_compile_key_tag14(data, len);

    if (report.entropy_milli < RAF_COMPILE_KEY_MIN_ENTROPY_MILLI) {
        report.status |= RAF_COMPILE_STATUS_LOW_ENTROPY;
    }
    if (report.tag14 == 0u) report.status |= RAF_COMPILE_STATUS_ZERO_TAG14;
    if (expected_crc32 != 0u && report.crc32 != expected_crc32) {
        report.status |= RAF_COMPILE_STATUS_CRC_MISMATCH;
    }

    return report;
}
