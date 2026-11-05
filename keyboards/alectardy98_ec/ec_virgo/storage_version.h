// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../common/runtime/storage_version.h"
// New EC42 allocation. Keep stable for compatible vial/vial_alec rebuilds.
#undef BUILD_ID
#define BUILD_ID ((uint32_t)0x0066ec42)
