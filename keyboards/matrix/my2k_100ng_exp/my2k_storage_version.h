/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "version.h"
/* The fork generates a random BUILD_ID each build. Vial uses its low24 bits as
 * the EEPROM validity marker, so random rebuilds also discard logical settings.
 * Both MY2K variants share storage ABI1. Change this ID only with an incompatible
 * EEPROM layout; tests compile the real layout for each variant. This is not a
 * manufacturer format or permission to enable physical flash persistence. */
#undef BUILD_ID
#define BUILD_ID ((uint32_t)0x004d5901)
