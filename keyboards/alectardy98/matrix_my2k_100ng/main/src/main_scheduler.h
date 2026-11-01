/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "ch.h"
#include <stdint.h>
/* Registers only the two existing persistent Main worker handles. */
void my2k_scheduler_workers(thread_t *host, thread_t *media);
/* Main housekeeping tail, unlocked: offer ready peers one scheduling turn. */
void my2k_main_cooperate(void);
/* Cached pre-yield scheduler state; no peripheral reads or actions on GET. */
void my2k_scheduler_status(uint8_t report[32]);
