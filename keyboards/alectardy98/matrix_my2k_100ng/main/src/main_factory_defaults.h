/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "main_nv32.h"
extern const uint8_t my2k_factory_image[MY2K_NV_BYTES];
bool my2k_factory_seed(my2k_nv32 *store);
