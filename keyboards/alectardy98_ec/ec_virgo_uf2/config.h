// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../ec_virgo/config.h"

// Separate TinyUF2 installation; EC42 logical record/UID are unchanged.
#undef A98_FIRMWARE_ID
#define A98_FIRMWARE_ID "A98-virgo-UF2-2.9g"
#undef WEAR_LEVELING_EFL_FIRST_SECTOR
#define WEAR_LEVELING_EFL_FIRST_SECTOR 2
#undef A98_STORAGE_START
#define A98_STORAGE_START 0x8000U
#undef A98_STORAGE_END
#define A98_STORAGE_END 0xc000U
#undef A98_APP_START
#define A98_APP_START 0x08010000U
#undef A98_TABLE_START
#define A98_TABLE_START 0x08010000U
#undef A98_IMAGE_END_SYMBOL
#define A98_IMAGE_END_SYMBOL __flash0_free__
