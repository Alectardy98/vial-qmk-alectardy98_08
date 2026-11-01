/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
enum {
    MY2K_PERF_SCAN,
    MY2K_PERF_HOUSEKEEPING,
    MY2K_PERF_RENDER,
    MY2K_PERF_RGB,
    MY2K_PERF_AUX,
    MY2K_PERF_EEPROM,
    MY2K_PERF_USB,
    MY2K_PERF_BUS,
#if MY2K_DEV15_PROFILE
    MY2K_PERF_STORAGE_QUEUE,
    MY2K_PERF_STORAGE_USB,
    MY2K_PERF_STORAGE_TX,
#endif
    MY2K_PERF_COUNT
};
typedef struct {
    uint32_t count, total_us, max_us, last_us, max_interval_us, last_interval_us, last_start;
    uint8_t  seen;
} my2k_perf_metric;
uint32_t         my2k_perf_clock(void);
void             my2k_perf_record(unsigned metric, uint32_t start);
my2k_perf_metric my2k_perf_get(unsigned metric);
