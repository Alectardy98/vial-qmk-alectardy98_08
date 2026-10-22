// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Fork-specific Vial identity; QMK's upstream metadata parser rejects this in
// config.h. post_config.h is included by QMK for every translation unit.
#define VIAL_KEYBOARD_UID {0x4D, 0x2D, 0x00, 0xBC, 0xAD, 0xF8, 0xB9, 0x27}
