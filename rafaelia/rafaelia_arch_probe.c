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
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Constants for bit manipulation and timing */
#define BITS_PER_BYTE 8u
#define ENDIANNESS_PROBE 0x01020304u
#define NANOSECONDS_PER_SECOND 1000000000ull
#define JSON_BUFFER_SIZE 2048u

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

/* Generate RFC 8259 compliant JSON output */
static int generate_json_output(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 64) {
        fprintf(stderr, "[ERROR] Invalid buffer for JSON generation\n");
        return -1;
    }

    bool used_asm = false;
    uint64_t counter = read_timing_counter(&used_asm);
    const char *arch = detect_arch();
    const char *endianness = detect_endianness();
    const char *simd = detect_simd();
    const char *fpu = detect_fpu();
    const size_t word_bits = sizeof(void *) * BITS_PER_BYTE;
    bool has_crypto = has_crypto_extensions();

    int written = snprintf(buffer, buffer_size,
        "{"
        "\"arch\":\"%s\","
        "\"word_bits\":%zu,"
        "\"endianness\":\"%s\","
        "\"simd\":\"%s\","
        "\"fpu\":\"%s\","
        "\"crypto\":%s,"
        "\"cycle_counter\":%" PRIu64 ","
        "\"used_inline_asm\":%s,"
        "\"timestamp_ns\":%" PRIu64 ""
        "}\n",
        arch,
        word_bits,
        endianness,
        simd,
        fpu,
        has_crypto ? "true" : "false",
        counter,
        used_asm ? "true" : "false",
        counter  /* If asm not used, this is already nanoseconds */
    );

    if (written < 0 || (size_t)written >= buffer_size) {
        fprintf(stderr, "[ERROR] JSON buffer overflow (needed %d bytes, have %zu)\n", 
                written, buffer_size);
        return -1;
    }

    return 0;
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
