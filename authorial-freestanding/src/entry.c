#include "raf_linux_syscall.h"

/* Explicit ELF entry. This is an ABI smoke test, not a PRoot replacement. */
__attribute__((noreturn)) void _start(void) {
    static const char message[] = "RAFAELIA_FREESTANDING_ABI_V1\n";
    long written = raf_linux_write(1U, message, sizeof(message) - 1U);
    raf_linux_exit(written == (long)(sizeof(message) - 1U) ? 0U : 74U);
}
