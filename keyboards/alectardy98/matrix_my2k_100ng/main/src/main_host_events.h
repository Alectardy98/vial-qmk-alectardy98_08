/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
/* First64 immutable diagnostic records; no retry or USB action on a query. */
void my2k_host_events_pass(bool entering);
void my2k_host_events_sleep_ms(uint32_t ms);
void my2k_host_events_summary(uint8_t report[32]);
bool my2k_host_events_page(uint8_t index,uint8_t report[32]);
