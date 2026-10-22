// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../common/runtime/storage_version.h"
// Preserve the physically tested clean-install Vial EEPROM identity.
// The fork otherwise generates a random BUILD_ID on every build.
// Change only for an intentionally incompatible Vial EEPROM layout.
#undef BUILD_ID
#define BUILD_ID ((uint32_t)0x0036E2FF)
