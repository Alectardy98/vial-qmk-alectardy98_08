/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "main_input_scheduler.h"
/* Injected only into two pure RGB calculation objects, never physical I2C.
 * No consumer can see a partial frame: flush follows completion in Main. */
#define MY2K_RENDER_CHECKPOINT() ((void)my2k_input_scheduler_checkpoint(true))
