/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "my2k_wire.h"
#include <stdbool.h>
#include <stdint.h>
/* Dev6-only read-only extension. No files, bus registers or flash are touched. */
void my2k_aux_telemetry_service(uint32_t now);
void my2k_aux_telemetry_receive(const my2k_wire_message *message, uint32_t now);
/* Returns a cached page; stale is explicit. Does not enqueue a UART query. */
void my2k_aux_telemetry_page(uint8_t page, uint8_t report[32], uint32_t now);

#if MY2K_DEV7_PROFILE
void my2k_aux_telemetry_counters(uint8_t report[32]);
#endif
