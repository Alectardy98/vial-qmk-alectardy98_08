/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "../vial/config.h"

/* Personal factory image; ordinary vial keeps its own defaults unchanged. */
#define MY2K_FACTORY_IMAGE_INCLUDE "keymaps/vial_alec/defaults/factory.inc"

/* Separate GUI definition/cache identity; persistent settings keep the same ABI. */
#undef VIAL_KEYBOARD_UID
#define VIAL_KEYBOARD_UID {0x4d, 0x59, 0x32, 0x4b, 0x45, 0x58, 0x50, 0x04}
#undef MY2K_DEV_BUILD
#define MY2K_DEV_BUILD "Release1-alec-defaults2"
