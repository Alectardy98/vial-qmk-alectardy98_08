/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
void my2k_rgb_trace_record(bool read, uint8_t address, uint8_t reg, const uint8_t *data, uint16_t count, int status,
                           uint8_t fault, uint8_t phase, uintptr_t caller, uint32_t now);
bool my2k_rgb_trace_page(uint8_t page, uint8_t index, uint8_t out[32]);
