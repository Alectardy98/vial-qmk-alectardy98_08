/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
void my2k_touchbar_init(void);
void my2k_touchbar_cancel(void);
void my2k_touchbar_sample(const uint8_t *, uint32_t);
void my2k_touchbar_service(uint32_t, bool);
bool my2k_touchbar_hid(uint8_t *, uint8_t, bool);

/* Retire the v1 touch slot only after preserving any valid legacy settings. */
bool my2k_touchbar_preserve_legacy(void);
