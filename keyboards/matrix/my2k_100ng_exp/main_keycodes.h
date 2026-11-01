/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "keycodes.h"
/* VialAstro customKeycodes[0] is USER00 = QK_KB_0, not SAFE_RANGE.
 * The resolved code is handled at any physical matrix position/layer. */
enum my2k_keycodes {
    MY2K_STORAGE_TOGGLE = QK_KB_0,
    MY2K_ANIMATION_NEXT,
    MY2K_ANIMATION_PREVIOUS,
    MY2K_ANIMATION_01,
    MY2K_ANIMATION_02,
    MY2K_ANIMATION_03,
    MY2K_ANIMATION_04,
    MY2K_ANIMATION_05,
    MY2K_ANIMATION_06,
    MY2K_ANIMATION_07,
    MY2K_ANIMATION_08,
    MY2K_ANIMATION_09,
    MY2K_ANIMATION_10,
    MY2K_NES_TOGGLE,
    MY2K_DOOM_TOGGLE,
    MY2K_NORMAL_MODE,
    MY2K_GBC_TOGGLE,
    MY2K_OLED_MODE_TOGGLE,
};
