#include "../core/librafaelia/impl/raf_arch_probe_core.h"

int main(void) {
    return raf_arch_probe_write_json(stdout, RAF_ARCH_PROBE_PROFILE_EXTENDED);
}
