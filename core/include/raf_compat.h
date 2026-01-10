/*
 * raf_compat.h - Cross-Architecture Compatibility Layer
 *
 * Copyright (c) 2024 Rafael Melo Reis
 * Licensed under RAFAELIA MODULE LICENSE
 *
 * This header provides compile-time and runtime architecture detection
 * for seamless interoperability across all supported platforms:
 * - Android (arm64-v8a, armeabi-v7a, x86_64, x86)
 * - Linux (x86_64, aarch64, arm, i386, riscv64, ppc64le)
 * - macOS (arm64, x86_64)
 *
 * Features:
 * - Automatic architecture detection via preprocessor
 * - API level compatibility checks for Android
 * - SIMD capability detection (NEON, SSE, AVX)
 * - Page size compatibility (4KB/16KB for Android 15+)
 * - NDK version compatibility
 * - Compiler feature detection (GCC, Clang, MSVC)
 *
 * Standards compliance:
 * - ISO/IEC 9899:2011 (C11)
 * - POSIX.1-2008
 */

#ifndef RAF_COMPAT_H
#define RAF_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Version Information
 * ===========================================================================*/
#define RAF_COMPAT_VERSION_MAJOR 1
#define RAF_COMPAT_VERSION_MINOR 0
#define RAF_COMPAT_VERSION_PATCH 0
#define RAF_COMPAT_VERSION \
    ((RAF_COMPAT_VERSION_MAJOR << 16) | \
     (RAF_COMPAT_VERSION_MINOR << 8) | \
     RAF_COMPAT_VERSION_PATCH)

/* =============================================================================
 * POSIX Feature Test Macros
 * ===========================================================================*/
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* =============================================================================
 * Standard Headers (minimal, portable)
 * ===========================================================================*/
#include <stdint.h>
#include <stddef.h>

/* =============================================================================
 * Architecture Detection
 * ===========================================================================*/

/* ARM64 / AArch64 */
#if defined(__aarch64__) || defined(__ARM_ARCH_8__) || defined(_M_ARM64)
#define RAF_ARCH_ARM64 1
#define RAF_ARCH_NAME "aarch64"
#define RAF_ARCH_BITS 64
#define RAF_ARCH_LITTLE_ENDIAN 1

/* ARM 32-bit */
#elif defined(__arm__) || defined(__ARM_ARCH_7A__) || defined(_M_ARM)
#define RAF_ARCH_ARM32 1
#define RAF_ARCH_NAME "arm"
#define RAF_ARCH_BITS 32
#define RAF_ARCH_LITTLE_ENDIAN 1

/* x86_64 / AMD64 */
#elif defined(__x86_64__) || defined(__amd64__) || defined(_M_X64) || defined(_M_AMD64)
#define RAF_ARCH_X86_64 1
#define RAF_ARCH_NAME "x86_64"
#define RAF_ARCH_BITS 64
#define RAF_ARCH_LITTLE_ENDIAN 1

/* x86 32-bit */
#elif defined(__i386__) || defined(__i686__) || defined(_M_IX86)
#define RAF_ARCH_X86 1
#define RAF_ARCH_NAME "x86"
#define RAF_ARCH_BITS 32
#define RAF_ARCH_LITTLE_ENDIAN 1

/* RISC-V 64-bit */
#elif defined(__riscv) && (__riscv_xlen == 64)
#define RAF_ARCH_RISCV64 1
#define RAF_ARCH_NAME "riscv64"
#define RAF_ARCH_BITS 64
#define RAF_ARCH_LITTLE_ENDIAN 1

/* RISC-V 32-bit */
#elif defined(__riscv) && (__riscv_xlen == 32)
#define RAF_ARCH_RISCV32 1
#define RAF_ARCH_NAME "riscv32"
#define RAF_ARCH_BITS 32
#define RAF_ARCH_LITTLE_ENDIAN 1

/* PowerPC 64-bit */
#elif defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__)
#define RAF_ARCH_PPC64 1
#define RAF_ARCH_NAME "ppc64"
#define RAF_ARCH_BITS 64
/* Check for little-endian PPC64 (ppc64le) using standard macros */
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define RAF_ARCH_LITTLE_ENDIAN 1
#else
#define RAF_ARCH_BIG_ENDIAN 1
#endif
#elif defined(_LITTLE_ENDIAN) || defined(__LITTLE_ENDIAN__)
#define RAF_ARCH_LITTLE_ENDIAN 1
#else
#define RAF_ARCH_BIG_ENDIAN 1
#endif

/* PowerPC 32-bit */
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#define RAF_ARCH_PPC 1
#define RAF_ARCH_NAME "ppc"
#define RAF_ARCH_BITS 32
#define RAF_ARCH_BIG_ENDIAN 1

/* MIPS 64-bit */
#elif defined(__mips64)
#define RAF_ARCH_MIPS64 1
#define RAF_ARCH_NAME "mips64"
#define RAF_ARCH_BITS 64
#define RAF_ARCH_BIG_ENDIAN 1

/* MIPS 32-bit */
#elif defined(__mips__)
#define RAF_ARCH_MIPS 1
#define RAF_ARCH_NAME "mips"
#define RAF_ARCH_BITS 32
#define RAF_ARCH_BIG_ENDIAN 1

/* s390x */
#elif defined(__s390x__)
#define RAF_ARCH_S390X 1
#define RAF_ARCH_NAME "s390x"
#define RAF_ARCH_BITS 64
#define RAF_ARCH_BIG_ENDIAN 1

/* Unknown architecture */
#else
#define RAF_ARCH_UNKNOWN 1
#define RAF_ARCH_NAME "unknown"
#define RAF_ARCH_BITS (sizeof(void*) * 8)
/* Runtime endianness detection needed */
#endif

/* =============================================================================
 * Operating System Detection
 * ===========================================================================*/

#if defined(__ANDROID__)
#define RAF_OS_ANDROID 1
#define RAF_OS_NAME "android"
#include <android/api-level.h>
#define RAF_ANDROID_API __ANDROID_API__
#elif defined(__linux__)
#define RAF_OS_LINUX 1
#define RAF_OS_NAME "linux"
#elif defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define RAF_OS_IOS 1
#define RAF_OS_NAME "ios"
#else
#define RAF_OS_MACOS 1
#define RAF_OS_NAME "macos"
#endif
#elif defined(_WIN32) || defined(_WIN64)
#define RAF_OS_WINDOWS 1
#define RAF_OS_NAME "windows"
#elif defined(__FreeBSD__)
#define RAF_OS_FREEBSD 1
#define RAF_OS_NAME "freebsd"
#elif defined(__OpenBSD__)
#define RAF_OS_OPENBSD 1
#define RAF_OS_NAME "openbsd"
#elif defined(__NetBSD__)
#define RAF_OS_NETBSD 1
#define RAF_OS_NAME "netbsd"
#else
#define RAF_OS_UNKNOWN 1
#define RAF_OS_NAME "unknown"
#endif

/* =============================================================================
 * Compiler Detection
 * ===========================================================================*/

#if defined(__clang__)
#define RAF_COMPILER_CLANG 1
#define RAF_COMPILER_NAME "clang"
#define RAF_COMPILER_VERSION \
    ((__clang_major__ * 10000) + (__clang_minor__ * 100) + __clang_patchlevel__)
#elif defined(__GNUC__)
#define RAF_COMPILER_GCC 1
#define RAF_COMPILER_NAME "gcc"
#define RAF_COMPILER_VERSION \
    ((__GNUC__ * 10000) + (__GNUC_MINOR__ * 100) + __GNUC_PATCHLEVEL__)
#elif defined(_MSC_VER)
#define RAF_COMPILER_MSVC 1
#define RAF_COMPILER_NAME "msvc"
#define RAF_COMPILER_VERSION _MSC_VER
#else
#define RAF_COMPILER_UNKNOWN 1
#define RAF_COMPILER_NAME "unknown"
#define RAF_COMPILER_VERSION 0
#endif

/* =============================================================================
 * SIMD Capability Detection
 * ===========================================================================*/

/* ARM NEON */
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define RAF_HAS_NEON 1
#endif

/* ARM Crypto extensions */
#if defined(__ARM_FEATURE_CRYPTO)
#define RAF_HAS_ARM_CRYPTO 1
#endif

/* ARM CRC32 */
#if defined(__ARM_FEATURE_CRC32)
#define RAF_HAS_ARM_CRC32 1
#endif

/* x86 SSE/AVX hierarchy */
#if defined(__AVX512F__)
#define RAF_HAS_AVX512 1
#define RAF_HAS_AVX2 1
#define RAF_HAS_AVX 1
#define RAF_HAS_SSE42 1
#define RAF_HAS_SSE41 1
#define RAF_HAS_SSSE3 1
#define RAF_HAS_SSE3 1
#define RAF_HAS_SSE2 1
#define RAF_HAS_SSE 1
#define RAF_SIMD_LEVEL 512
#define RAF_SIMD_NAME "avx512"
#elif defined(__AVX2__)
#define RAF_HAS_AVX2 1
#define RAF_HAS_AVX 1
#define RAF_HAS_SSE42 1
#define RAF_HAS_SSE41 1
#define RAF_HAS_SSSE3 1
#define RAF_HAS_SSE3 1
#define RAF_HAS_SSE2 1
#define RAF_HAS_SSE 1
#define RAF_SIMD_LEVEL 256
#define RAF_SIMD_NAME "avx2"
#elif defined(__AVX__)
#define RAF_HAS_AVX 1
#define RAF_HAS_SSE42 1
#define RAF_HAS_SSE41 1
#define RAF_HAS_SSSE3 1
#define RAF_HAS_SSE3 1
#define RAF_HAS_SSE2 1
#define RAF_HAS_SSE 1
#define RAF_SIMD_LEVEL 256
#define RAF_SIMD_NAME "avx"
#elif defined(__SSE4_2__)
#define RAF_HAS_SSE42 1
#define RAF_HAS_SSE41 1
#define RAF_HAS_SSSE3 1
#define RAF_HAS_SSE3 1
#define RAF_HAS_SSE2 1
#define RAF_HAS_SSE 1
#define RAF_SIMD_LEVEL 128
#define RAF_SIMD_NAME "sse4.2"
#elif defined(__SSE4_1__)
#define RAF_HAS_SSE41 1
#define RAF_HAS_SSSE3 1
#define RAF_HAS_SSE3 1
#define RAF_HAS_SSE2 1
#define RAF_HAS_SSE 1
#define RAF_SIMD_LEVEL 128
#define RAF_SIMD_NAME "sse4.1"
#elif defined(__SSSE3__)
#define RAF_HAS_SSSE3 1
#define RAF_HAS_SSE3 1
#define RAF_HAS_SSE2 1
#define RAF_HAS_SSE 1
#define RAF_SIMD_LEVEL 128
#define RAF_SIMD_NAME "ssse3"
#elif defined(__SSE3__)
#define RAF_HAS_SSE3 1
#define RAF_HAS_SSE2 1
#define RAF_HAS_SSE 1
#define RAF_SIMD_LEVEL 128
#define RAF_SIMD_NAME "sse3"
#elif defined(__SSE2__)
#define RAF_HAS_SSE2 1
#define RAF_HAS_SSE 1
#define RAF_SIMD_LEVEL 128
#define RAF_SIMD_NAME "sse2"
#elif defined(__SSE__)
#define RAF_HAS_SSE 1
#define RAF_SIMD_LEVEL 128
#define RAF_SIMD_NAME "sse"
#elif defined(RAF_HAS_NEON)
#define RAF_SIMD_LEVEL 128
#define RAF_SIMD_NAME "neon"
#else
#define RAF_SIMD_LEVEL 0
#define RAF_SIMD_NAME "none"
#endif

/* x86 AES-NI */
#if defined(__AES__)
#define RAF_HAS_AES_NI 1
#endif

/* RISC-V Vector Extension */
#if defined(__riscv_vector)
#define RAF_HAS_RVV 1
#endif

/* PowerPC AltiVec */
#if defined(__ALTIVEC__)
#define RAF_HAS_ALTIVEC 1
#endif

/* =============================================================================
 * Android API Level Compatibility Macros
 * ===========================================================================*/

#ifdef RAF_OS_ANDROID

/* Android 15 (API 35) - 16KB page size support */
#if RAF_ANDROID_API >= 35
#define RAF_ANDROID_15_COMPAT 1
#define RAF_PAGE_SIZE 16384
#else
#define RAF_PAGE_SIZE 4096
#endif

/* Android 14 (API 34) */
#if RAF_ANDROID_API >= 34
#define RAF_ANDROID_14_COMPAT 1
#endif

/* Android 13 (API 33) */
#if RAF_ANDROID_API >= 33
#define RAF_ANDROID_13_COMPAT 1
#endif

/* Android 12 (API 31/32) */
#if RAF_ANDROID_API >= 31
#define RAF_ANDROID_12_COMPAT 1
#endif

/* Android 11 (API 30) */
#if RAF_ANDROID_API >= 30
#define RAF_ANDROID_11_COMPAT 1
#endif

/* Android 10 (API 29) */
#if RAF_ANDROID_API >= 29
#define RAF_ANDROID_10_COMPAT 1
#endif

/* Minimum supported API level */
#define RAF_MIN_ANDROID_API 21

#else /* Non-Android */
/* Default to standard page size */
#ifndef RAF_PAGE_SIZE
#define RAF_PAGE_SIZE 4096
#endif
#endif /* RAF_OS_ANDROID */

/* =============================================================================
 * NDK Version Compatibility
 * ===========================================================================*/

#ifdef RAF_OS_ANDROID
/* Minimum recommended NDK version for Android 15 compatibility */
#define RAF_MIN_NDK_VERSION 27

/* Check for NDK version if available */
#if defined(__NDK_MAJOR__)
#define RAF_NDK_VERSION __NDK_MAJOR__
#if RAF_NDK_VERSION < RAF_MIN_NDK_VERSION
#warning "NDK version is older than recommended. Consider upgrading to NDK r27+"
#endif
#endif
#endif /* RAF_OS_ANDROID */

/* =============================================================================
 * Compiler Attribute Macros
 * ===========================================================================*/

/* Function visibility */
#if defined(RAF_COMPILER_GCC) || defined(RAF_COMPILER_CLANG)
#define RAF_PUBLIC __attribute__((visibility("default")))
#define RAF_PRIVATE __attribute__((visibility("hidden")))
#define RAF_INLINE static inline __attribute__((always_inline))
#define RAF_NOINLINE __attribute__((noinline))
#define RAF_UNUSED __attribute__((unused))
#define RAF_PURE __attribute__((pure))
#define RAF_CONST __attribute__((const))
#define RAF_NORETURN __attribute__((noreturn))
#define RAF_LIKELY(x) __builtin_expect(!!(x), 1)
#define RAF_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define RAF_ALIGNED(n) __attribute__((aligned(n)))
#define RAF_PACKED __attribute__((packed))
#define RAF_DEPRECATED __attribute__((deprecated))
#define RAF_FORMAT(archetype, string_index, first_to_check) \
    __attribute__((format(archetype, string_index, first_to_check)))
#elif defined(RAF_COMPILER_MSVC)
#define RAF_PUBLIC __declspec(dllexport)
#define RAF_PRIVATE
#define RAF_INLINE static __forceinline
#define RAF_NOINLINE __declspec(noinline)
#define RAF_UNUSED
#define RAF_PURE
#define RAF_CONST
#define RAF_NORETURN __declspec(noreturn)
#define RAF_LIKELY(x) (x)
#define RAF_UNLIKELY(x) (x)
#define RAF_ALIGNED(n) __declspec(align(n))
#define RAF_PACKED
#define RAF_DEPRECATED __declspec(deprecated)
#define RAF_FORMAT(archetype, string_index, first_to_check)
#else
#define RAF_PUBLIC
#define RAF_PRIVATE
#define RAF_INLINE static inline
#define RAF_NOINLINE
#define RAF_UNUSED
#define RAF_PURE
#define RAF_CONST
#define RAF_NORETURN
#define RAF_LIKELY(x) (x)
#define RAF_UNLIKELY(x) (x)
#define RAF_ALIGNED(n)
#define RAF_PACKED
#define RAF_DEPRECATED
#define RAF_FORMAT(archetype, string_index, first_to_check)
#endif

/* Thread-local storage */
#if defined(RAF_COMPILER_GCC) || defined(RAF_COMPILER_CLANG)
#define RAF_THREAD_LOCAL __thread
#elif defined(RAF_COMPILER_MSVC)
#define RAF_THREAD_LOCAL __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define RAF_THREAD_LOCAL _Thread_local
#else
#define RAF_THREAD_LOCAL
#endif

/* =============================================================================
 * Type Size Assertions
 * ===========================================================================*/

/* Compile-time assertion macro */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define RAF_STATIC_ASSERT(expr, msg) _Static_assert(expr, msg)
#elif defined(__COUNTER__)
/* Use __COUNTER__ for unique names when available (GCC, Clang, MSVC) */
#define RAF_STATIC_ASSERT_JOIN_(a, b) a##b
#define RAF_STATIC_ASSERT_JOIN(a, b) RAF_STATIC_ASSERT_JOIN_(a, b)
#define RAF_STATIC_ASSERT(expr, msg) \
    typedef char RAF_STATIC_ASSERT_JOIN(raf_static_assert_, __COUNTER__)[(expr) ? 1 : -1]
#else
/* Fallback using __LINE__ - may have issues with multiple assertions on same line */
#define RAF_STATIC_ASSERT(expr, msg) \
    typedef char RAF_STATIC_ASSERT_##__LINE__[(expr) ? 1 : -1]
#endif

/* Verify expected type sizes */
RAF_STATIC_ASSERT(sizeof(uint8_t) == 1, "uint8_t must be 1 byte");
RAF_STATIC_ASSERT(sizeof(uint16_t) == 2, "uint16_t must be 2 bytes");
RAF_STATIC_ASSERT(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
RAF_STATIC_ASSERT(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");

/* =============================================================================
 * Runtime Architecture Detection Functions
 * ===========================================================================*/

/**
 * Get compile-time detected architecture name
 * @return Architecture name string (e.g., "aarch64", "x86_64")
 */
RAF_INLINE const char* raf_get_arch_name(void) {
    return RAF_ARCH_NAME;
}

/**
 * Get compile-time detected OS name
 * @return OS name string (e.g., "android", "linux", "macos")
 */
RAF_INLINE const char* raf_get_os_name(void) {
    return RAF_OS_NAME;
}

/**
 * Get compile-time detected compiler name
 * @return Compiler name string (e.g., "clang", "gcc")
 */
RAF_INLINE const char* raf_get_compiler_name(void) {
    return RAF_COMPILER_NAME;
}

/**
 * Get word size in bits
 * @return Word size (32 or 64 typically)
 */
RAF_INLINE int raf_get_word_bits(void) {
    return RAF_ARCH_BITS;
}

/**
 * Get SIMD capability level in bits
 * @return SIMD width (0, 128, 256, or 512)
 */
RAF_INLINE int raf_get_simd_level(void) {
    return RAF_SIMD_LEVEL;
}

/**
 * Get SIMD capability name
 * @return SIMD name string (e.g., "neon", "avx2", "none")
 */
RAF_INLINE const char* raf_get_simd_name(void) {
    return RAF_SIMD_NAME;
}

/**
 * Check if running on little-endian system
 * @return 1 if little-endian, 0 if big-endian
 */
RAF_INLINE int raf_is_little_endian(void) {
#if defined(RAF_ARCH_LITTLE_ENDIAN)
    return 1;
#elif defined(RAF_ARCH_BIG_ENDIAN)
    return 0;
#else
    /* Runtime detection for unknown architectures */
    const uint16_t probe = 0x0102;
    const uint8_t* bytes = (const uint8_t*)&probe;
    return bytes[0] == 0x02;
#endif
}

/**
 * Get page size for current platform
 * @return Page size in bytes (4096 or 16384 typically)
 */
RAF_INLINE size_t raf_get_page_size(void) {
    return RAF_PAGE_SIZE;
}

#ifdef RAF_OS_ANDROID
/**
 * Get Android API level
 * @return API level (e.g., 35 for Android 15)
 */
RAF_INLINE int raf_get_android_api(void) {
    return RAF_ANDROID_API;
}

/**
 * Check if running on Android 15 or later
 * @return 1 if Android 15+, 0 otherwise
 */
RAF_INLINE int raf_is_android_15_plus(void) {
#ifdef RAF_ANDROID_15_COMPAT
    return 1;
#else
    return 0;
#endif
}
#endif /* RAF_OS_ANDROID */

/* =============================================================================
 * Version Compatibility Utilities
 * ===========================================================================*/

/**
 * Compare two version numbers
 * @param v1_major Major version 1
 * @param v1_minor Minor version 1
 * @param v1_patch Patch version 1
 * @param v2_major Major version 2
 * @param v2_minor Minor version 2
 * @param v2_patch Patch version 2
 * @return -1 if v1 < v2, 0 if equal, 1 if v1 > v2
 */
RAF_INLINE int raf_version_compare(
    int v1_major, int v1_minor, int v1_patch,
    int v2_major, int v2_minor, int v2_patch) {
    
    if (v1_major != v2_major) {
        return (v1_major > v2_major) ? 1 : -1;
    }
    if (v1_minor != v2_minor) {
        return (v1_minor > v2_minor) ? 1 : -1;
    }
    if (v1_patch != v2_patch) {
        return (v1_patch > v2_patch) ? 1 : -1;
    }
    return 0;
}

/**
 * Check if compat header version is at least the specified version
 * @param major Required major version
 * @param minor Required minor version
 * @param patch Required patch version
 * @return 1 if current version is >= required, 0 otherwise
 */
RAF_INLINE int raf_compat_version_at_least(int major, int minor, int patch) {
    return raf_version_compare(
        RAF_COMPAT_VERSION_MAJOR,
        RAF_COMPAT_VERSION_MINOR,
        RAF_COMPAT_VERSION_PATCH,
        major, minor, patch) >= 0;
}

#ifdef __cplusplus
}
#endif

#endif /* RAF_COMPAT_H */
