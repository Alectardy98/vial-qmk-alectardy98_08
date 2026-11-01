/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct {
 uint32_t bytes,checkpoints,sends,failed_sends,frames,failed_frames;
 uint32_t frame_started_ms,last_frame_ms,max_frame_ms,total_frame_ms;
 uint32_t histogram[5]; /* <=8,<=16,<=24,<=33,>33 ms; actual completed transfer */
 bool frame_active;
} aux_display_perf21;
extern aux_display_perf21 aux_display_perf;

#if MY2K_DEV22_PROFILE
typedef struct {
 uint32_t flushes,pages,failures,last_cycles,max_cycles,last_ms,max_ms;
 uint32_t started_ms,current_cycles;
} aux_mono_perf_stats;
extern aux_mono_perf_stats aux_mono_perf;
#endif
