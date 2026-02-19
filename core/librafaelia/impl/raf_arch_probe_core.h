#ifndef RAF_ARCH_PROBE_CORE_H
#define RAF_ARCH_PROBE_CORE_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum raf_arch_probe_profile_e {
    RAF_ARCH_PROBE_PROFILE_COMPACT = 0,
    RAF_ARCH_PROBE_PROFILE_EXTENDED = 1
} raf_arch_probe_profile_t;

int raf_arch_probe_write_json(FILE *out, raf_arch_probe_profile_t profile);

#ifdef __cplusplus
}
#endif

#endif
