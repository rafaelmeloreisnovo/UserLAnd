#define _POSIX_C_SOURCE 200809L

#include "rafaelia_hw_map.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/auxv.h>
#endif

#ifndef RAFAELIA_MMIO_BASE_0
#define RAFAELIA_MMIO_BASE_0 0xFE000000ull
#endif
#ifndef RAFAELIA_MMIO_BASE_1
#define RAFAELIA_MMIO_BASE_1 0x3F000000ull
#endif
#ifndef RAFAELIA_MMIO_BASE_2
#define RAFAELIA_MMIO_BASE_2 0x10000000ull
#endif

#define MAX_ITEMS 12u
#define ITEM_LEN 64u

static void append_item(char items[][ITEM_LEN], size_t *count, const char *value) {
    if (!items || !count || !value || *count >= MAX_ITEMS) {
        return;
    }
    (void)snprintf(items[*count], ITEM_LEN, "%s", value);
    *count += 1u;
}

static bool file_readable(const char *path) {
    struct stat st;
    return (path && stat(path, &st) == 0);
}

static void collect_safe_registers(char items[][ITEM_LEN], size_t *count) {
    if (file_readable("/proc/cpuinfo")) {
        append_item(items, count, "cpuinfo_regs");
    }
    if (file_readable("/proc/interrupts")) {
        append_item(items, count, "interrupt_timers");
    }
#if defined(__linux__)
    {
        errno = 0;
        if (getauxval(16ul) != 0ul || errno == 0) {
            append_item(items, count, "auxv_hwcap");
        }
    }
#endif
    if (file_readable("/sys/bus/event_source/devices/cpu")) {
        append_item(items, count, "pmu_cpu");
    }
    if (file_readable("/sys/devices/system/cpu")) {
        append_item(items, count, "cpu_topology");
    }
}

static void collect_safe_gpio(char items[][ITEM_LEN], size_t *count) {
    if (file_readable("/sys/class/gpio")) {
        append_item(items, count, "sysfs_gpio");
    }
    if (file_readable("/sys/kernel/debug/pinctrl")) {
        append_item(items, count, "debugfs_pinctrl");
    }
    if (file_readable("/proc/device-tree")) {
        append_item(items, count, "devicetree_gpio_nodes");
    }
}

#if defined(RAFAELIA_ENABLE_PRIVILEGED_MMIO)
static bool read_mmio_probe(uint64_t base) {
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        return false;
    }

    const uint64_t page_mask = (uint64_t)page_size - 1ull;
    const off_t page_base = (off_t)(base & ~page_mask);
    const off_t page_off = (off_t)(base & page_mask);

    int fd = open("/dev/mem", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    void *mapped = mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, page_base);
    if (mapped == MAP_FAILED) {
        close(fd);
        return false;
    }

    volatile const uint32_t *reg = (volatile const uint32_t *)((const unsigned char *)mapped + page_off);
    volatile uint32_t value = *reg;
    (void)value;

    munmap(mapped, (size_t)page_size);
    close(fd);
    return true;
}

static void collect_privileged_mmio(char items[][ITEM_LEN], size_t *count) {
    const char *enable = getenv("RAFAELIA_MMIO_ENABLE");
    if (!enable || strcmp(enable, "1") != 0) {
        return;
    }

    if (read_mmio_probe(RAFAELIA_MMIO_BASE_0)) {
        append_item(items, count, "mmio_timer_block");
    }
    if (read_mmio_probe(RAFAELIA_MMIO_BASE_1)) {
        append_item(items, count, "mmio_gpio_block");
    }
    if (read_mmio_probe(RAFAELIA_MMIO_BASE_2)) {
        append_item(items, count, "mmio_soc_id");
    }
}
#endif

#if !defined(RAFAELIA_ENABLE_PRIVILEGED_MMIO)
static void collect_privileged_mmio(char items[][ITEM_LEN], size_t *count) {
    (void)items;
    (void)count;
}
#endif

static int write_json_array(char *buffer, size_t buffer_size, int pos, char items[][ITEM_LEN], size_t count) {
    int written = snprintf(buffer + pos, buffer_size - (size_t)pos, "[");
    if (written < 0 || (size_t)(pos + written) >= buffer_size) {
        return -1;
    }
    pos += written;

    for (size_t i = 0; i < count; i++) {
        written = snprintf(buffer + pos, buffer_size - (size_t)pos, "%s\"%s\"", (i == 0u) ? "" : ",", items[i]);
        if (written < 0 || (size_t)(pos + written) >= buffer_size) {
            return -1;
        }
        pos += written;
    }

    written = snprintf(buffer + pos, buffer_size - (size_t)pos, "]");
    if (written < 0 || (size_t)(pos + written) >= buffer_size) {
        return -1;
    }
    return pos + written;
}

int rafaelia_hw_map_generate_json(char *buffer, size_t buffer_size) {
    char registers[MAX_ITEMS][ITEM_LEN] = {{0}};
    char gpio[MAX_ITEMS][ITEM_LEN] = {{0}};
    size_t reg_count = 0u;
    size_t gpio_count = 0u;
    int pos = 0;
    int written;
    const char *access_level = "userland_safe";

    if (!buffer || buffer_size < 64u) {
        return -1;
    }

    collect_safe_registers(registers, &reg_count);
    collect_safe_gpio(gpio, &gpio_count);

    {
        const size_t before = reg_count;
        collect_privileged_mmio(registers, &reg_count);
        if (reg_count > before) {
            access_level = "privileged_mmio";
        }
    }

    written = snprintf(buffer + pos, buffer_size - (size_t)pos, "{");
    if (written < 0 || (size_t)(pos + written) >= buffer_size) return -1;
    pos += written;

    written = snprintf(buffer + pos, buffer_size - (size_t)pos, "\"registers_detected\":");
    if (written < 0 || (size_t)(pos + written) >= buffer_size) return -1;
    pos += written;

    pos = write_json_array(buffer, buffer_size, pos, registers, reg_count);
    if (pos < 0) return -1;

    written = snprintf(buffer + pos, buffer_size - (size_t)pos, ",\"gpio_pinctrl_detected\":");
    if (written < 0 || (size_t)(pos + written) >= buffer_size) return -1;
    pos += written;

    pos = write_json_array(buffer, buffer_size, pos, gpio, gpio_count);
    if (pos < 0) return -1;

    written = snprintf(buffer + pos, buffer_size - (size_t)pos, ",\"access_level\":\"%s\"}", access_level);
    if (written < 0 || (size_t)(pos + written) >= buffer_size) return -1;

    return 0;
}
