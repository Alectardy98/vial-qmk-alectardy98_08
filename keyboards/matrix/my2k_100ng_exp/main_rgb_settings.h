/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "my2k_rgb_settings.h"
typedef struct {
    uint8_t  bytes[MY2K_RGB_SETTINGS_BYTES];
    uint32_t saved_revision;
#if MY2K_LOCK_LIGHTS_PROFILE
    bool restored;
#endif
#if MY2K_DEV9_PROFILE
    /* A versioned private QMK user-word policy, saved by the same journal. */
    bool brightness_policy_pending;
#endif
} my2k_main_rgb_settings;
/* Bind explicit RGB serialization to QMK's reserved keyboard data block, not
 * guessed EEPROM addresses. The QMK custom driver owns deferred durability. */
void my2k_main_rgb_restore(my2k_main_rgb_settings *, my2k_amk_rgb_runtime *);
void my2k_main_rgb_save_task(my2k_main_rgb_settings *, const my2k_amk_rgb_runtime *);
