/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
/* Exclusive elapsed CPU cycles including interrupts. Nesting separates SD from
 * FatFs, file access from decoding, and worker suspension from SPI. This is not
 * CPU utilization or LCD scanout timing. No ISR calls this single-owner profiler. */
enum { PERF22_DECODE,PERF22_FILE,PERF22_SD,PERF22_PANEL,PERF22_SPI,PERF22_SUSPENDED,PERF22_GAP,PERF22_PHASES };
enum { PERF22_FILE_CALL,PERF22_SD_CALL,PERF22_PIXEL_CALL,PERF22_SEND_CALL,PERF22_YIELD_CALL,PERF22_SEEK_CALL,PERF22_COUNTS };
typedef struct {uint32_t cycles[PERF22_PHASES],elapsed,sequence;uint16_t count[PERF22_COUNTS],interval_ms;uint8_t valid,variant;} aux_perf22_frame;
void aux_perf22_init(void);
bool aux_perf22_enter(unsigned phase);
void aux_perf22_leave(bool entered);
void aux_perf22_count(unsigned counter);
void aux_perf22_begin(uint16_t interval_ms);
void aux_perf22_end(bool success);
bool aux_perf22_page(uint8_t page,uint8_t out[8]);
#ifdef MY2K_PERF22_HOST
extern uint32_t aux_perf22_host_clock;
#endif
