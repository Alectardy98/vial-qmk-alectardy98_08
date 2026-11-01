/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef bool (*my2k_refresh_write)(uint8_t chip, uint8_t reg, const uint8_t *data, uint8_t length);
typedef struct {
    uint32_t transactions, bytes, failures, completed, staged, coalesced;
} my2k_refresh_stats;
void my2k_refresh_init(void);
void my2k_refresh_set(unsigned chip, unsigned channel, uint8_t value);
bool my2k_refresh_pending(void);
/* At most ONE existing-bus transaction; never waits for the rest of a frame.
 * Failed chips retry after100ms; no unbounded retry or hidden init. */
bool               my2k_refresh_step(uint8_t allowed, uint32_t now, my2k_refresh_write write);
my2k_refresh_stats my2k_refresh_diagnostics(void);
void               my2k_refresh_staged(void);
