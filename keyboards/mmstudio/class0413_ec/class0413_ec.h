// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "quantum.h"
enum class0413_ec_keycodes {
    CLASS0413_EC_NOISE = QK_KB_0,
    CLASS0413_EC_BOTTOM_START,
    CLASS0413_EC_CAL_PRINT,
    CLASS0413_LK_SONG,
};

void ec_lock_song_toggle(void);
