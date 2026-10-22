// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../common/runtime/storage_version.h"

// Selected per keymap in post_rules.mk: vial and vial_alec have different
// layout-options/keymap/macro offsets. Never share their compatibility ID.
// These values retain the last audited clean builds' Vial EEPROM identities.
#ifndef A98_MATRIX_VIAL_BUILD_ID
#    error "Select the Matrix keymap's EEPROM compatibility ID in post_rules.mk"
#endif
#undef BUILD_ID
#define BUILD_ID ((uint32_t)A98_MATRIX_VIAL_BUILD_ID)
