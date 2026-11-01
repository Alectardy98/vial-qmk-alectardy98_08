/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#if MY2K_ACTIVE_DEV_PROFILE && !MY2K_INPUT_SCHEDULER_HOST && !MY2K_RGB_HW_HOST
void my2k_input_io_init(void);
bool my2k_input_io_begin(void);
void my2k_input_io_end(void);
bool my2k_input_io_status(uint8_t report[32]);
#else
static inline void my2k_input_io_init(void){}
static inline bool my2k_input_io_begin(void){return false;}
static inline void my2k_input_io_end(void){}
#endif
