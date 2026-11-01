/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "my2k_storage_version.h"
#include "raw_hid.h"
/* Only QMK via.c is compiled with this rename. The board-level entry validates
 * fixed HID size and the malformed zero-byte misaligned-write edge first. */
void my2k_qmk_raw_hid_receive(uint8_t *, uint8_t);
#define raw_hid_receive my2k_qmk_raw_hid_receive
#if MY2K_DEV2_PROFILE
/* Actual startup validity path retained for power-cycle regression tests. */
void via_init(void) __attribute__((noinline, noclone, externally_visible));
#endif
