/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Cooperative task-context service only. It never configures an interrupt or
 * timer. Caller must establish a genuinely safe checkpoint (no held software
 * lock; transport pause legal) before passing safe_point=true. */
void my2k_input_scheduler_reset(void);
void my2k_input_scheduler_scan(uint32_t cycles);
void my2k_input_scheduler_enter(bool high_performance,bool keyboard_ready,bool storage_mode);
void my2k_input_scheduler_leave(void);
bool my2k_input_scheduler_checkpoint(bool safe_point);
/* Main-only DMA window admission; not a generic permission to reenter QMK. */
bool my2k_input_scheduler_can_io(void);
unsigned my2k_input_scheduler_wait_us(void);
bool my2k_input_scheduler_status(uint8_t index,uint8_t report[32]);
#if MY2K_INPUT_SCHEDULER_HOST
uint32_t my2k_input_scheduler_clock(void);
bool my2k_input_scheduler_context_ready(void);
void my2k_input_scheduler_keyboard_task(void);
#endif
