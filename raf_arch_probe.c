#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static uint64_t read_cycle_counter(bool *used_asm) {
#if defined(__aarch64__)
  uint64_t vct;
  __asm__ volatile("mrs %0, cntvct_el0" : "=r"(vct));
  if (used_asm) {
    *used_asm = true;
  }
  return vct;
#elif defined(__x86_64__) || defined(__i386__)
  uint32_t lo, hi;
  __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
  if (used_asm) {
    *used_asm = true;
  }
  return ((uint64_t)hi << 32) | lo;
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  if (used_asm) {
    *used_asm = false;
  }
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
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
  const uint16_t probe = 0x0102;
  const unsigned char *b = (const unsigned char *)&probe;
  return b[0] == 0x01 ? "big" : "little";
}

int main(void) {
  bool used_asm = false;
  uint64_t counter = read_cycle_counter(&used_asm);
  const char *arch = detect_arch();
  const char *endianness = detect_endianness();
  const size_t word_bits = sizeof(void *) * 8u;

  printf(
      "{"
      "\"arch\":\"%s\","
      "\"word_bits\":%zu,"
      "\"endianness\":\"%s\","
      "\"cycle_counter\":%" PRIu64 ","
      "\"used_inline_asm\":%s"
      "}\n",
      arch, word_bits, endianness, counter, used_asm ? "true" : "false");

  return 0;
}
