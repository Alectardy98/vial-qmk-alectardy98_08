/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "../release1locks/config.h"
#undef MY2K_DEV_BUILD
#include "dev_build.h"
#undef EEPROM_SIZE
#define EEPROM_SIZE 32768
/* Last256 bytes belong to board settings; VIA macros cannot alias them. */
#define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR 32511
