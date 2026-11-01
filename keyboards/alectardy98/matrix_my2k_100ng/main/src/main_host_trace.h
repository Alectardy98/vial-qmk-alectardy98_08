/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
/* Dev10 passive evidence only: existing host work never depends on this cache.
 * Service reads non-pop registers at most once/20ms. HID handlers use RAM only. */
void my2k_host_trace_service(uint32_t now_ms);
void my2k_host_trace_loop(bool entering);
bool my2k_host_trace_page(uint8_t index, uint8_t report[32]);
bool my2k_host_channel_page(uint8_t index, uint8_t report[32]);
