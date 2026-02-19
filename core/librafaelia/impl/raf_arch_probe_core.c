#define _POSIX_C_SOURCE 200809L

#include "raf_arch_probe_core.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(__linux__)
#include <errno.h>
#include <sys/auxv.h>
#if defined(__aarch64__) || defined(__arm__)
#include <asm/hwcap.h>
#endif
#endif

#define BITS_PER_BYTE 8u
#define ENDIANNESS_PROBE 0x01020304u
#define NANOSECONDS_PER_SECOND 1000000000ull
#define JSON_BUFFER_SIZE 2304u

typedef struct runtime_caps_s {
    const char *arch_runtime;
    const char *simd_runtime;
    const char *crypto_runtime;
    const char *fpu_runtime;
} runtime_caps_t;

static const char *detect_arch(void) {
#if defined(__aarch64__) || defined(__ARM_ARCH_8__)
    return "aarch64";
#elif defined(__arm__) || defined(__ARM_ARCH_7A__)
    return "arm";
#elif defined(__x86_64__) || defined(__amd64__)
    return "x86_64";
#elif defined(__i386__) || defined(__i686__)
    return "x86";
#elif defined(__riscv) && (__riscv_xlen == 64)
    return "riscv64";
#elif defined(__riscv) && (__riscv_xlen == 32)
    return "riscv32";
#elif defined(__powerpc64__)
    return "ppc64";
#elif defined(__powerpc__)
    return "ppc";
#elif defined(__mips64)
    return "mips64";
#elif defined(__mips__)
    return "mips";
#elif defined(__s390x__)
    return "s390x";
#elif defined(__alpha__)
    return "alpha";
#elif defined(__sparc__)
    return "sparc";
#else
    return "unknown";
#endif
}

static const char *detect_endianness(void) {
    const uint32_t probe = ENDIANNESS_PROBE;
    const unsigned char *bytes = (const unsigned char *)&probe;

    if (bytes[0] == 0x01) return "big";
    if (bytes[0] == 0x04) return "little";
    return "mixed";
}

static const char *detect_simd_compile(void) {
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    return "neon";
#elif defined(__AVX512F__)
    return "avx512";
#elif defined(__AVX2__)
    return "avx2";
#elif defined(__AVX__)
    return "avx";
#elif defined(__SSE4_2__)
    return "sse4.2";
#elif defined(__SSE4_1__)
    return "sse4.1";
#elif defined(__SSSE3__)
    return "ssse3";
#elif defined(__SSE3__)
    return "sse3";
#elif defined(__SSE2__)
    return "sse2";
#elif defined(__SSE__)
    return "sse";
#elif defined(__riscv_vector)
    return "rvv";
#elif defined(__ALTIVEC__)
    return "altivec";
#else
    return "none";
#endif
}

static const char *detect_fpu_compile(void) {
#if defined(__ARM_FP)
    return "arm_vfp";
#elif defined(__FPU_PRESENT) && __FPU_PRESENT
    return "present";
#elif defined(__x86_64__) || defined(__i386__)
    return "x87_sse";
#elif defined(__riscv_flen)
    return "riscv_f";
#else
    return "soft";
#endif
}

static bool detect_crypto_compile(void) {
#if defined(__ARM_FEATURE_CRYPTO) || defined(__AES__)
    return true;
#else
    return false;
#endif
}

static uint64_t read_timing_counter(bool *used_asm) {
    bool asm_used = false;
    uint64_t value = 0;

#if defined(__aarch64__)
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(value));
    asm_used = true;
#elif defined(__x86_64__) || defined(__i386__)
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    value = ((uint64_t)hi << 32) | lo;
    asm_used = true;
#elif defined(__riscv)
    __asm__ volatile("rdtime %0" : "=r"(value));
    asm_used = true;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        value = (uint64_t)ts.tv_sec * NANOSECONDS_PER_SECOND + (uint64_t)ts.tv_nsec;
    }
#endif

    if (used_asm != NULL) *used_asm = asm_used;
    return value;
}

static void detect_runtime_caps(runtime_caps_t *caps, raf_arch_probe_profile_t profile) {
    caps->arch_runtime = detect_arch();
    caps->simd_runtime = detect_simd_compile();
    caps->crypto_runtime = detect_crypto_compile() ? "true" : "false";
    caps->fpu_runtime = detect_fpu_compile();

    if (profile != RAF_ARCH_PROBE_PROFILE_EXTENDED) return;

#if defined(__x86_64__) || defined(__i386__)
    uint32_t eax = 1u, ebx = 0u, ecx = 0u, edx = 0u;
    bool has_fpu = false, has_sse = false, has_sse2 = false, has_sse3 = false;
    bool has_ssse3 = false, has_sse41 = false, has_sse42 = false;
    bool has_avx = false, has_avx2 = false, has_aes = false, has_osxsave = false;
    uint64_t xcr0 = 0;

    __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : : "cc");
    has_fpu = (edx & (1u << 0)) != 0u;
    has_sse = (edx & (1u << 25)) != 0u;
    has_sse2 = (edx & (1u << 26)) != 0u;
    has_sse3 = (ecx & (1u << 0)) != 0u;
    has_ssse3 = (ecx & (1u << 9)) != 0u;
    has_sse41 = (ecx & (1u << 19)) != 0u;
    has_sse42 = (ecx & (1u << 20)) != 0u;
    has_aes = (ecx & (1u << 25)) != 0u;
    has_avx = (ecx & (1u << 28)) != 0u;
    has_osxsave = (ecx & (1u << 27)) != 0u;

    if (has_osxsave) {
        uint32_t xcr0_lo = 0, xcr0_hi = 0;
        __asm__ volatile("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0) : "cc");
        xcr0 = ((uint64_t)xcr0_hi << 32) | xcr0_lo;
    }

    eax = 7u;
    ecx = 0u;
    __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx) : : "cc");
    has_avx2 = (ebx & (1u << 5)) != 0u;

    if (has_avx && ((xcr0 & 0x6u) != 0x6u)) has_avx = false;
    if (has_avx2 && !has_avx) has_avx2 = false;

    caps->simd_runtime = has_avx2 ? "avx2" :
                         has_avx ? "avx" :
                         has_sse42 ? "sse4.2" :
                         has_sse41 ? "sse4.1" :
                         has_ssse3 ? "ssse3" :
                         has_sse3 ? "sse3" :
                         has_sse2 ? "sse2" :
                         has_sse ? "sse" : "none";
    caps->crypto_runtime = has_aes ? "true" : "false";
    caps->fpu_runtime = has_fpu ? "present" : "soft";

#elif defined(__linux__) && (defined(__aarch64__) || defined(__arm__)) && defined(AT_HWCAP)
    unsigned long hwcap = 0ul;
#if defined(AT_HWCAP2)
    unsigned long hwcap2 = 0ul;
#endif
    bool has_neon = false;
    bool has_fpu = false;
    bool has_aes = false;

    errno = 0;
    hwcap = getauxval(AT_HWCAP);
    if (errno != 0) return;
#if defined(AT_HWCAP2)
    errno = 0;
    hwcap2 = getauxval(AT_HWCAP2);
    if (errno != 0) hwcap2 = 0ul;
#endif

#if defined(__aarch64__)
#ifdef HWCAP_ASIMD
    has_neon = (hwcap & HWCAP_ASIMD) != 0ul;
#endif
#ifdef HWCAP_FP
    has_fpu = (hwcap & HWCAP_FP) != 0ul;
#endif
#ifdef HWCAP_AES
    has_aes = (hwcap & HWCAP_AES) != 0ul;
#endif
#else
#ifdef HWCAP_NEON
    has_neon = (hwcap & HWCAP_NEON) != 0ul;
#endif
#ifdef HWCAP_VFP
    has_fpu = (hwcap & HWCAP_VFP) != 0ul;
#endif
#ifdef HWCAP2_AES
    has_aes = (hwcap2 & HWCAP2_AES) != 0ul;
#endif
#endif

    caps->simd_runtime = has_neon ? "neon" : "none";
    caps->fpu_runtime = has_fpu ? "present" : "soft";
    caps->crypto_runtime = has_aes ? "true" : "false";
#endif
}

int raf_arch_probe_write_json(FILE *out, raf_arch_probe_profile_t profile) {
    runtime_caps_t runtime_caps;
    bool used_asm = false;
    char buffer[JSON_BUFFER_SIZE];
    const char *arch = detect_arch();
    const char *endianness = detect_endianness();
    const char *simd = detect_simd_compile();
    const char *fpu = detect_fpu_compile();
    const bool crypto = detect_crypto_compile();
    const uint64_t counter = read_timing_counter(&used_asm);
    const char *profile_name = profile == RAF_ARCH_PROBE_PROFILE_EXTENDED ? "extended" : "compact";
    const size_t word_bits = sizeof(void *) * BITS_PER_BYTE;
    int written;

    if (out == NULL) return 1;

    detect_runtime_caps(&runtime_caps, profile);

    written = snprintf(
        buffer,
        sizeof(buffer),
        "{\"profile\":\"%s\","
        "\"arch\":\"%s\","
        "\"arch_compiled_for\":\"%s\","
        "\"arch_runtime\":\"%s\","
        "\"word_bits\":%zu,"
        "\"endianness\":\"%s\","
        "\"simd\":\"%s\","
        "\"simd_runtime\":\"%s\","
        "\"fpu\":\"%s\","
        "\"fpu_runtime\":\"%s\","
        "\"crypto\":%s,"
        "\"crypto_runtime\":%s,"
        "\"cycle_counter\":%" PRIu64 ","
        "\"used_inline_asm\":%s,"
        "\"timestamp_ns\":%" PRIu64 "}\n",
        profile_name,
        arch,
        arch,
        runtime_caps.arch_runtime,
        word_bits,
        endianness,
        simd,
        runtime_caps.simd_runtime,
        fpu,
        runtime_caps.fpu_runtime,
        crypto ? "true" : "false",
        strcmp(runtime_caps.crypto_runtime, "true") == 0 ? "true" : "false",
        counter,
        used_asm ? "true" : "false",
        counter);

    if (written <= 0 || (size_t)written >= sizeof(buffer)) return 1;
    return fputs(buffer, out) == EOF ? 1 : 0;
}
