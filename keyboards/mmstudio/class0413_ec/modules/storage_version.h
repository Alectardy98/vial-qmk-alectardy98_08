// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "version.h"
/* Keep the tested S2 Vial EEPROM schema ID, not QMK's random per-build ID.
 * Confirmed in reference ELF via_eeprom_is_valid: bytes 20 57 17 at 0x100..102.
 * Bump only for an intentionally incompatible EEPROM layout change.
 */
#undef BUILD_ID
#define BUILD_ID ((uint32_t)0x00175720)
