/*
 * raf_minimal.h - Minimal common definitions for RAF core utilities
 * 
 * This header provides inline implementations of commonly needed functions
 * to eliminate dependencies on stdlib.h and ctype.h while maintaining
 * portability and performance.
 * 
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RAF_MINIMAL_H
#define RAF_MINIMAL_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * Inline bool type definition (replaces stdbool.h)
 */
#ifndef __cplusplus
typedef uint8_t bool;
#define true  1
#define false 0
#endif

/*
 * Get current time in milliseconds (POSIX monotonic clock)
 */
static inline uint64_t raf_now_ms(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    return 0;
  }
  return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

/*
 * Get monotonic time in nanoseconds
 */
static inline uint64_t raf_now_ns(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/*
 * Parse unsigned 64-bit integer from string (replaces strtoull)
 * Returns parsed value and advances cursor pointer
 * Includes overflow protection
 */
static inline uint64_t raf_parse_u64(const char **cursor) {
  uint64_t value = 0;
  while (**cursor >= '0' && **cursor <= '9') {
    uint64_t digit = **cursor - '0';
    // Check for overflow
    if (value > UINT64_MAX / 10) {
      break;  // would overflow
    }
    uint64_t new_value = value * 10;
    if (new_value > UINT64_MAX - digit) {
      break;  // would overflow
    }
    value = new_value + digit;
    (*cursor)++;
  }
  return value;
}

/*
 * Parse unsigned 32-bit integer from string (replaces strtoul)
 * Returns parsed value and advances cursor pointer
 * Includes overflow protection
 */
static inline uint32_t raf_parse_u32(const char **cursor) {
  uint32_t value = 0;
  while (**cursor >= '0' && **cursor <= '9') {
    uint32_t digit = **cursor - '0';
    // Check for overflow
    if (value > UINT32_MAX / 10) {
      break;  // would overflow
    }
    uint32_t new_value = value * 10;
    if (new_value > UINT32_MAX - digit) {
      break;  // would overflow
    }
    value = new_value + digit;
    (*cursor)++;
  }
  return value;
}

/*
 * Skip whitespace characters (replaces isspace)
 */
static inline void raf_skip_space(const char **cursor) {
  while (**cursor == ' ' || **cursor == '\t') {
    (*cursor)++;
  }
}

/*
 * Check if character is whitespace (replaces isspace)
 */
static inline bool raf_is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/*
 * Check if character is a digit (replaces isdigit)
 */
static inline bool raf_is_digit(char c) {
  return c >= '0' && c <= '9';
}

/*
 * Check if character is alphanumeric (replaces isalnum)
 */
static inline bool raf_is_alnum(char c) {
  return (c >= '0' && c <= '9') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= 'a' && c <= 'z');
}

/*
 * Drain a long line from input stream (skip to newline)
 * Returns 1 if line was incomplete (had to drain), 0 otherwise
 */
static inline int raf_drain_line(FILE *fp, char *line) {
  size_t len = strlen(line);
  if (len > 0 && line[len - 1] == '\n') {
    return 0;
  }
  int ch = 0;
  while ((ch = fgetc(fp)) != EOF) {
    if (ch == '\n') {
      break;
    }
  }
  return 1;
}

/*
 * Safe string copy with bounds checking
 * Returns number of characters copied (excluding null terminator)
 */
static inline size_t raf_strlcpy(char *dst, const char *src, size_t size) {
  size_t i;
  for (i = 0; i < size - 1 && src[i] != '\0'; i++) {
    dst[i] = src[i];
  }
  if (size > 0) {
    dst[i] = '\0';
  }
  return i;
}

/*
 * Architecture-specific cycle counter reading
 * Returns hardware cycle counter if available, otherwise nanosecond timestamp
 * Sets *used_asm to true if inline assembly was used
 */
static inline uint64_t raf_read_cycles(bool *used_asm) {
  bool asm_flag = false;
  uint64_t value = 0;
  
#if defined(__aarch64__)
  __asm__ volatile("mrs %0, cntvct_el0" : "=r"(value));
  asm_flag = true;
  
#elif defined(__x86_64__) || defined(__i386__)
  uint32_t lo, hi;
  __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
  asm_flag = true;
  value = ((uint64_t)hi << 32) | lo;
  
#else
  // Fallback: use monotonic clock
  value = raf_now_ns();
  
#endif

  if (used_asm) {
    *used_asm = asm_flag;
  }
  return value;
}

/*
 * Detect CPU architecture at compile time
 */
static inline const char *raf_detect_arch(void) {
#if defined(__aarch64__)
  return "aarch64";
#elif defined(__arm__)
  return "arm";
#elif defined(__x86_64__)
  return "x86_64";
#elif defined(__i386__)
  return "x86";
#elif defined(__riscv) && (__riscv_xlen == 64)
  return "riscv64";
#elif defined(__riscv) && (__riscv_xlen == 32)
  return "riscv32";
#else
  return "unknown";
#endif
}

/*
 * Detect byte order (endianness)
 */
static inline const char *raf_detect_endian(void) {
  const uint16_t probe = 0x0102;
  const unsigned char *b = (const unsigned char *)&probe;
  return b[0] == 0x01 ? "big" : "little";
}

/*
 * Get word size in bits
 */
static inline size_t raf_word_bits(void) {
  return sizeof(void *) * 8;
}

#endif /* RAF_MINIMAL_H */
