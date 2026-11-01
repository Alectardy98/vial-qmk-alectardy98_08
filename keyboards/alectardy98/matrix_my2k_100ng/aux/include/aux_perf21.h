/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct {
    uint32_t bytes, checkpoints, sends, failed_sends, frames, failed_frames;
    uint32_t frame_started_ms, last_frame_ms, max_frame_ms, total_frame_ms;
    uint32_t histogram[5]; /* <=8,<=16,<=24,<=33,>33 ms; actual completed transfer */
    bool     frame_active;
} aux_display_perf21;
extern aux_display_perf21 aux_display_perf;
typedef struct {
    uint32_t panels_begin_ms, panels_ready_ms, storage_begin_ms, storage_ready_ms;
    uint32_t selection_ms, first_frame_ms;
} aux_startup_timing;
extern aux_startup_timing aux_startup;

#if MY2K_DEV22_PROFILE
typedef struct {
    uint32_t flushes, pages, failures, last_cycles, max_cycles, last_ms, max_ms;
    uint32_t started_ms, current_cycles;
} aux_mono_perf_stats;
extern aux_mono_perf_stats aux_mono_perf;
#endif
