/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
bool my2k_debug_hid(uint8_t *, unsigned, bool);
void my2k_debug_task(uint32_t);
void my2k_debug_host_irq(void *, unsigned, uint32_t);
