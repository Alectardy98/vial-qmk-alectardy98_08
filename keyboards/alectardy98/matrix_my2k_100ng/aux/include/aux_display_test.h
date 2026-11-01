/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "aux_assets.h"
void aux_display_test_set(bool on, uint32_t now);
bool aux_display_test_active(void);
bool aux_display_test_runnable(uint32_t now);
void aux_display_test_status(uint8_t out[4]);
void aux_display_test_step(uint32_t now, aux_pixel_sink sink, void *context);
