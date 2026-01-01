// Optimized low-level architecture probe - minimal dependencies
// Removed: inttypes.h, stdbool.h (replaced with inline definitions)
// Benefits: Reduced binary size, faster compilation
#define _POSIX_C_SOURCE 199309L

#include <stdint.h>
#include <stdio.h>
#include <time.h>

// Inline bool type definition to avoid stdbool.h
typedef uint8_t bool;
#define true 1
#define false 0

#define BITS_PER_BYTE 8u
#define ENDIANNESS_PROBE 0x0102u  // Distinct byte pattern for endian detection
#define NANOSECONDS_PER_SECOND 1000000000ull

static uint64_t read_timing_counter(bool *used_asm) {
  bool asm_used = false;
  uint64_t value = 0;
#if defined(__aarch64__)
  __asm__ volatile("mrs %0, cntvct_el0" : "=r"(value));
  asm_used = true;
#elif defined(__x86_64__) || defined(__i386__)
  uint32_t lo, hi;
  __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
  asm_used = true;
  value = ((uint64_t)hi << 32) | lo;
#else
  struct timespec ts;
  // Fallback: monotonic clock in nanoseconds (not a hardware cycle counter) when
  // arch-specific timers are unavailable.
  clock_gettime(CLOCK_MONOTONIC, &ts);
  value = (uint64_t)ts.tv_sec * NANOSECONDS_PER_SECOND +
          (uint64_t)ts.tv_nsec;
#endif

  if (used_asm) {
    *used_asm = asm_used;
  }
  return value;
}

static const char *detect_arch(void) {
#if defined(__aarch64__)
  return "aarch64";
#elif defined(__arm__)
  return "arm";
#elif defined(__x86_64__)
  return "x86_64";
#elif defined(__i386__)
  return "x86";
#else
  return "unknown";
#endif
}

static const char *detect_endianness(void) {
  const uint16_t probe = ENDIANNESS_PROBE;
  const unsigned char *b = (const unsigned char *)&probe;
  return b[0] == 0x01 ? "big" : "little";
}

int main(void) {
  bool used_asm = false;
  uint64_t counter = read_timing_counter(&used_asm);
  const char *arch = detect_arch();
  const char *endianness = detect_endianness();
  const size_t word_bits = sizeof(void *) * BITS_PER_BYTE;

  printf("{\"arch\":\"%s\",", arch);
  printf("\"word_bits\":%zu,", word_bits);
  printf("\"endianness\":\"%s\",", endianness);
  printf("\"cycle_counter\":%llu,", (unsigned long long)counter);
  printf("\"used_inline_asm\":%s", used_asm ? "true" : "false");
  printf("}\n");

  return 0;
}
