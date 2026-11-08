#include "raf_linux_syscall.h"

/* The kernel owns the ABI. All instructions below are architecture-specific.
 * No instruction sequence is claimed to bypass Android process sandboxing. */
long raf_linux_write(unsigned int fd, const void *bytes, raf_size_t count) {
#if defined(__aarch64__)
    register long a0 __asm__("x0") = (long)fd;
    register long a1 __asm__("x1") = (long)bytes;
    register long a2 __asm__("x2") = (long)count;
    register long nr __asm__("x8") = 64L; /* __NR_write */
    __asm__ volatile("svc #0" : "+r"(a0) : "r"(a1), "r"(a2), "r"(nr) : "memory", "cc");
    return a0;
#elif defined(__arm__)
    register long a0 __asm__("r0") = (long)fd;
    register long a1 __asm__("r1") = (long)bytes;
    register long a2 __asm__("r2") = (long)count;
    register long nr __asm__("r7") = 4L;  /* __NR_write */
    __asm__ volatile("svc #0" : "+r"(a0) : "r"(a1), "r"(a2), "r"(nr) : "memory", "cc");
    return a0;
#elif defined(__x86_64__) && defined(__linux__)
    register long a0 __asm__("rax") = 1L; /* __NR_write */
    __asm__ volatile("syscall" : "+a"(a0) : "D"((long)fd), "S"(bytes), "d"((long)count) : "rcx", "r11", "memory");
    return a0;
#else
#error "Unsupported kernel ABI: only Linux ARMv7, AArch64 and host x86_64 are covered"
#endif
}

__attribute__((noreturn)) void raf_linux_exit(unsigned int status) {
#if defined(__aarch64__)
    register long a0 __asm__("x0") = (long)status;
    register long nr __asm__("x8") = 93L; /* __NR_exit */
    __asm__ volatile("svc #0" : : "r"(a0), "r"(nr) : "memory", "cc");
#elif defined(__arm__)
    register long a0 __asm__("r0") = (long)status;
    register long nr __asm__("r7") = 1L;  /* __NR_exit */
    __asm__ volatile("svc #0" : : "r"(a0), "r"(nr) : "memory", "cc");
#elif defined(__x86_64__) && defined(__linux__)
    __asm__ volatile("syscall" : : "a"(60L), "D"((long)status) : "rcx", "r11", "memory");
#else
#error "Unsupported kernel ABI"
#endif
    __builtin_unreachable();
}
