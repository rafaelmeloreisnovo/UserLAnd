/*
 * rafaelia_arch_probe.c - Hardware-Aware Architecture Probe Helper
 * 
 * Copyright (c) 2024 Rafael Melo Reis
 * Licensed under RAFAELIA MODULE LICENSE (see LICENSE file)
 *
 * This module provides low-level hardware architecture detection with
 * comprehensive JSON output conforming to W3C and IETF standards.
 *
 * Function blocks:
 * - Architecture detection (aarch64, arm, x86_64, x86, riscv, etc.)
 * - Endianness detection (big-endian, little-endian)
 * - Word size detection (32-bit, 64-bit, 128-bit capable)
 * - Hardware capability detection (SIMD, FPU, crypto extensions)
 * - Timing counter access (TSC, CNTVCT, fallback to CLOCK_MONOTONIC)
 * - JSON output generation (RFC 8259 compliant)
 *
 * Mitigations:
 * - Buffer overflow protection via bounded writes
 * - Invalid pointer handling
 * - Architecture-specific fallbacks for unsupported features
 * - Graceful degradation when hardware features unavailable
 *
 * Adaptations:
 * - Compile-time architecture detection
 * - Runtime capability probing
 * - Cross-platform compatibility (Linux, Android, *BSD)
 * - Minimal dependencies (C11 standard library only)
 *
 * Interoperability:
 * - JSON output format (RFC 8259)
 * - Standard exit codes (0 = success, non-zero = error)
 * - Pipe-friendly output (stdout for data, stderr for errors)
 *
 * Standards compliance:
 * - ISO/IEC 9899:2011 (C11)
 * - POSIX.1-2008
 * - RFC 8259 (JSON)
 * - IEEE 754 (Floating-Point Arithmetic)
 */

#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(__linux__)
#include <elf.h>
#include <sys/auxv.h>
#if defined(__aarch64__) || defined(__arm__)
#include <asm/hwcap.h>
#endif
#endif

/* Constants for bit manipulation and timing */
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

/* Architecture detection at compile time */
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

/* Endianness detection using union-based probing */
static const char *detect_endianness(void) {
    const uint32_t probe = ENDIANNESS_PROBE;
    const unsigned char *bytes = (const unsigned char *)&probe;
    
    if (bytes[0] == 0x01) {
        return "big";
    } else if (bytes[0] == 0x04) {
        return "little";
    } else {
        return "mixed";  /* Rare PDP-endian or other */
    }
}

/* Read hardware timing counter with architecture-specific assembly */
static uint64_t read_timing_counter(bool *used_asm) {
    bool asm_used = false;
    uint64_t value = 0;

#if defined(__aarch64__)
    /* ARM64: Use CNTVCT_EL0 (virtual count register) */
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(value));
    asm_used = true;
#elif defined(__x86_64__) || defined(__i386__)
    /* x86/x64: Use RDTSC (timestamp counter) */
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    value = ((uint64_t)hi << 32) | lo;
    asm_used = true;
#elif defined(__riscv)
    /* RISC-V: Use RDTIME (time register) */
    __asm__ volatile("rdtime %0" : "=r"(value));
    asm_used = true;
#else
    /* Fallback: POSIX monotonic clock (nanoseconds, not cycle counter) */
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        value = (uint64_t)ts.tv_sec * NANOSECONDS_PER_SECOND + (uint64_t)ts.tv_nsec;
    }
#endif

    if (used_asm) {
        *used_asm = asm_used;
    }
    return value;
}

/* Detect SIMD capabilities at compile time */
static const char *detect_simd(void) {
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

/* Detect floating-point capabilities */
static const char *detect_fpu(void) {
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

/* Detect hardware crypto extensions */
static bool has_crypto_extensions(void) {
#if defined(__ARM_FEATURE_CRYPTO)
    return true;
#elif defined(__AES__)
    return true;
#else
    return false;
#endif
}

#if defined(__riscv)
static bool str_contains_ci(const char *haystack, const char *needle) {
    size_t i;
    const size_t hay_len = strlen(haystack);
    const size_t needle_len = strlen(needle);

    if (needle_len == 0 || hay_len < needle_len) {
        return false;
    }

    for (i = 0; i <= hay_len - needle_len; i++) {
        size_t j;
        bool matched = true;
        for (j = 0; j < needle_len; j++) {
            char a = haystack[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return true;
        }
    }

    return false;
}
#endif

static void detect_runtime_caps(runtime_caps_t *caps) {
    if (!caps) {
        return;
    }

    caps->arch_runtime = "unknown";
    caps->simd_runtime = "unknown";
    caps->crypto_runtime = "unknown";
    caps->fpu_runtime = "unknown";

#if defined(__x86_64__) || defined(__i386__)
    uint32_t eax, ebx, ecx, edx;
    bool cpuid_ok = false;
    bool has_fpu = false;
    bool has_sse = false;
    bool has_sse2 = false;
    bool has_sse3 = false;
    bool has_ssse3 = false;
    bool has_sse41 = false;
    bool has_sse42 = false;
    bool has_avx = false;
    bool has_avx2 = false;
    bool has_aes = false;
    bool has_osxsave = false;
    uint64_t xcr0 = 0;

    eax = 0u;
    __asm__ volatile(
        "cpuid"
        : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        :
        : "cc");

    if (eax >= 7u) {
        cpuid_ok = true;
        caps->arch_runtime = detect_arch();
    }

    if (!cpuid_ok) {
        return;
    }

    eax = 1u;
    __asm__ volatile(
        "cpuid"
        : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        :
        : "cc");

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
        uint32_t xcr0_lo = 0;
        uint32_t xcr0_hi = 0;
        __asm__ volatile("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0) : "cc");
        xcr0 = ((uint64_t)xcr0_hi << 32) | xcr0_lo;
    }

    eax = 7u;
    ecx = 0u;
    __asm__ volatile(
        "cpuid"
        : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx)
        :
        : "cc");
    has_avx2 = (ebx & (1u << 5)) != 0u;

    if (has_avx && ((xcr0 & 0x6u) != 0x6u)) {
        has_avx = false;
    }
    if (has_avx2 && !has_avx) {
        has_avx2 = false;
    }

    caps->fpu_runtime = has_fpu ? "present" : "soft";
    caps->crypto_runtime = has_aes ? "aes" : "none";

    if (has_avx2) caps->simd_runtime = "avx2";
    else if (has_avx) caps->simd_runtime = "avx";
    else if (has_sse42) caps->simd_runtime = "sse4.2";
    else if (has_sse41) caps->simd_runtime = "sse4.1";
    else if (has_ssse3) caps->simd_runtime = "ssse3";
    else if (has_sse3) caps->simd_runtime = "sse3";
    else if (has_sse2) caps->simd_runtime = "sse2";
    else if (has_sse) caps->simd_runtime = "sse";
    else caps->simd_runtime = "none";

#elif defined(__aarch64__) || defined(__arm__)
#if defined(__linux__) && defined(AT_HWCAP)
    {
        bool auxv_ok = false;
        unsigned long hwcap;
        errno = 0;
        hwcap = getauxval(AT_HWCAP);
        if (errno == 0) {
            auxv_ok = true;
            caps->arch_runtime = detect_arch();
        }

        if (!auxv_ok) {
            return;
        }
#if defined(AT_HWCAP2)
        const unsigned long hwcap2 = getauxval(AT_HWCAP2);
#else
        const unsigned long hwcap2 = 0ul;
#endif
        bool has_asimd = false;
        bool has_fpu = false;
        bool has_aes = false;
        bool has_sha = false;
        bool has_pmull = false;

#if defined(__aarch64__)
#ifdef HWCAP_ASIMD
        has_asimd = (hwcap & HWCAP_ASIMD) != 0ul;
#endif
#ifdef HWCAP_FP
        has_fpu = (hwcap & HWCAP_FP) != 0ul;
#endif
#ifdef HWCAP_AES
        has_aes = (hwcap & HWCAP_AES) != 0ul;
#endif
#ifdef HWCAP_SHA1
        has_sha = has_sha || ((hwcap & HWCAP_SHA1) != 0ul);
#endif
#ifdef HWCAP_SHA2
        has_sha = has_sha || ((hwcap & HWCAP_SHA2) != 0ul);
#endif
#ifdef HWCAP_PMULL
        has_pmull = (hwcap & HWCAP_PMULL) != 0ul;
#endif
#else
#ifdef HWCAP_NEON
        has_asimd = (hwcap & HWCAP_NEON) != 0ul;
#endif
#ifdef HWCAP_VFP
        has_fpu = (hwcap & HWCAP_VFP) != 0ul;
#endif
#ifdef HWCAP2_AES
        has_aes = (hwcap2 & HWCAP2_AES) != 0ul;
#endif
#ifdef HWCAP2_SHA1
        has_sha = has_sha || ((hwcap2 & HWCAP2_SHA1) != 0ul);
#endif
#ifdef HWCAP2_SHA2
        has_sha = has_sha || ((hwcap2 & HWCAP2_SHA2) != 0ul);
#endif
#ifdef HWCAP2_PMULL
        has_pmull = (hwcap2 & HWCAP2_PMULL) != 0ul;
#endif
#endif

        caps->simd_runtime = has_asimd ? "neon" : "none";
        caps->fpu_runtime = has_fpu ? "present" : "soft";
        if (has_aes && has_sha && has_pmull) caps->crypto_runtime = "aes+sha+pmull";
        else if (has_aes && has_sha) caps->crypto_runtime = "aes+sha";
        else if (has_aes) caps->crypto_runtime = "aes";
        else if (has_sha) caps->crypto_runtime = "sha";
        else if (has_pmull) caps->crypto_runtime = "pmull";
        else caps->crypto_runtime = "none";
    }
#else
    caps->simd_runtime = "unknown";
    caps->crypto_runtime = "unknown";
    caps->fpu_runtime = "unknown";
#endif

#elif defined(__riscv)
    {
        FILE *f = fopen("/proc/cpuinfo", "r");
        char line[512];
        bool cpuinfo_ok = false;
        bool found_line = false;
        bool has_v = false;
        bool has_zk = false;

        if (f) {
            while (fgets(line, sizeof(line), f) != NULL) {
                if (str_contains_ci(line, "isa") || str_contains_ci(line, "features")) {
                    found_line = true;
                    if (str_contains_ci(line, "_v") || str_contains_ci(line, " v") || str_contains_ci(line, "v ")) {
                        has_v = true;
                    }
                    if (str_contains_ci(line, "zk")) {
                        has_zk = true;
                    }
                }
            }
            fclose(f);
        }

        if (f && found_line) {
            cpuinfo_ok = true;
            caps->arch_runtime = detect_arch();
        }

        if (!cpuinfo_ok) {
            caps->simd_runtime = "unknown";
            caps->crypto_runtime = "unknown";
            caps->fpu_runtime = "unknown";
        } else {
            caps->simd_runtime = has_v ? "rvv" : "none";
            caps->crypto_runtime = has_zk ? "zk" : "none";
            caps->fpu_runtime = "unknown";
        }
    }
#else
    caps->arch_runtime = "unknown";
    caps->simd_runtime = "unknown";
    caps->crypto_runtime = "unknown";
    caps->fpu_runtime = "unknown";
#endif
}

/* Generate RFC 8259 compliant JSON output */
static int generate_json_output(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 64) {
        fprintf(stderr, "[ERROR] Invalid buffer for JSON generation\n");
        return -1;
    }

    bool used_asm = false;
    uint64_t counter = read_timing_counter(&used_asm);
    const char *arch_compiled_for = detect_arch();
    const char *endianness = detect_endianness();
    const char *simd = detect_simd();
    const char *fpu = detect_fpu();
    const size_t word_bits = sizeof(void *) * BITS_PER_BYTE;
    bool has_crypto = has_crypto_extensions();
    runtime_caps_t runtime_caps;
    detect_runtime_caps(&runtime_caps);

    /* Build JSON incrementally for better maintainability and error handling */
    int pos = 0;
    int written;
    
    /* JSON opening brace */
    written = snprintf(buffer + pos, buffer_size - pos, "{");
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;
    
    /* Architecture field */
    written = snprintf(buffer + pos, buffer_size - pos, "\"arch\":\"%s\",", arch_compiled_for);
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;

    written = snprintf(buffer + pos, buffer_size - pos, "\"arch_compiled_for\":\"%s\",", arch_compiled_for);
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;

    written = snprintf(buffer + pos, buffer_size - pos, "\"arch_runtime\":\"%s\",", runtime_caps.arch_runtime);
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;
    
    /* Word bits field */
    written = snprintf(buffer + pos, buffer_size - pos, "\"word_bits\":%zu,", word_bits);
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;
    
    /* Endianness field */
    written = snprintf(buffer + pos, buffer_size - pos, "\"endianness\":\"%s\",", endianness);
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;
    
    /* SIMD field */
    written = snprintf(buffer + pos, buffer_size - pos, "\"simd\":\"%s\",", simd);
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;

    written = snprintf(buffer + pos, buffer_size - pos, "\"simd_runtime\":\"%s\",", runtime_caps.simd_runtime);
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;
    
    /* FPU field */
    written = snprintf(buffer + pos, buffer_size - pos, "\"fpu\":\"%s\",", fpu);
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;

    written = snprintf(buffer + pos, buffer_size - pos, "\"fpu_runtime\":\"%s\",", runtime_caps.fpu_runtime);
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;
    
    /* Crypto field */
    written = snprintf(buffer + pos, buffer_size - pos, "\"crypto\":%s,", has_crypto ? "true" : "false");
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;

    written = snprintf(buffer + pos, buffer_size - pos, "\"crypto_runtime\":\"%s\",", runtime_caps.crypto_runtime);
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;
    
    /* Cycle counter field */
    written = snprintf(buffer + pos, buffer_size - pos, "\"cycle_counter\":%" PRIu64 ",", counter);
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;
    
    /* Inline ASM field */
    written = snprintf(buffer + pos, buffer_size - pos, "\"used_inline_asm\":%s,", used_asm ? "true" : "false");
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;
    
    /* Timestamp field (last, no trailing comma) */
    written = snprintf(buffer + pos, buffer_size - pos, "\"timestamp_ns\":%" PRIu64, counter);
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;
    
    /* JSON closing brace and newline */
    written = snprintf(buffer + pos, buffer_size - pos, "}\n");
    if (written < 0 || (size_t)(pos + written) >= buffer_size) goto overflow;
    pos += written;

    return 0;

overflow:
    fprintf(stderr, "[ERROR] JSON buffer overflow (needed more than %zu bytes)\n", buffer_size);
    return -1;
}

/*
 * Main entry point
 * Returns: 0 on success, non-zero on error
 * Output: JSON to stdout, errors to stderr
 */
int main(void) {
    char json_buffer[JSON_BUFFER_SIZE];
    
    if (generate_json_output(json_buffer, sizeof(json_buffer)) != 0) {
        return 1;
    }

    /* Write JSON to stdout (pipe-friendly, single write syscall) */
    if (fputs(json_buffer, stdout) == EOF) {
        fprintf(stderr, "[ERROR] Failed to write JSON output\n");
        return 1;
    }

    return 0;
}
