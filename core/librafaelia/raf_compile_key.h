#ifndef RAFAELIA_COMPILE_KEY_H
#define RAFAELIA_COMPILE_KEY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RAF_COMPILE_KEY_MAX_LEN 64u
#define RAF_COMPILE_KEY_MIN_LEN 8u
#define RAF_COMPILE_KEY_MIN_ENTROPY_MILLI 3500u

#define RAF_COMPILE_FLAG_LICENSE_ACK 0x00000001u
#define RAF_COMPILE_FLAG_COPYRIGHT_ACK 0x00000002u
#define RAF_COMPILE_FLAG_NO_PRIVILEGE_ESCALATION 0x00000004u
#define RAF_COMPILE_FLAG_NO_EXTERNAL_NETWORK 0x00000008u
#define RAF_COMPILE_FLAGS_REQUIRED \
    (RAF_COMPILE_FLAG_LICENSE_ACK | RAF_COMPILE_FLAG_COPYRIGHT_ACK | RAF_COMPILE_FLAG_NO_PRIVILEGE_ESCALATION)

#define RAF_COMPILE_STATUS_OK 0x00000000u
#define RAF_COMPILE_STATUS_NULL_INPUT 0x00000001u
#define RAF_COMPILE_STATUS_LENGTH_SHORT 0x00000002u
#define RAF_COMPILE_STATUS_LENGTH_LONG 0x00000004u
#define RAF_COMPILE_STATUS_LOW_ENTROPY 0x00000008u
#define RAF_COMPILE_STATUS_ZERO_TAG14 0x00000010u
#define RAF_COMPILE_STATUS_CRC_MISMATCH 0x00000020u
#define RAF_COMPILE_STATUS_MISSING_LICENSE_FLAGS 0x00000040u

typedef struct RafCompileKeyReport {
    uint32_t status;
    uint32_t crc32;
    uint16_t entropy_milli;
    uint16_t tag14;
} RafCompileKeyReport;

uint32_t raf_compile_key_crc32(const uint8_t *data, uint32_t len);
uint16_t raf_compile_key_entropy_milli(const uint8_t *data, uint32_t len);
uint16_t raf_compile_key_tag14(const uint8_t *data, uint32_t len);
RafCompileKeyReport raf_compile_key_validate(const uint8_t *data,
                                              uint32_t len,
                                              uint32_t flags,
                                              uint32_t expected_crc32);

#ifdef __cplusplus
}
#endif

#endif  // RAFAELIA_COMPILE_KEY_H
