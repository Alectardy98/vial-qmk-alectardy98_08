/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_rgb_settings.h"
#include "eeconfig.h"
#if MY2K_DEV16_PROFILE
#    include "main_performance_mode.h"
#endif
#if MY2K_DEV9_PROFILE
/* MY-2K has no user eeconfig consumer. Own this existing QMK word, rather
 * than shifting the 592-byte keyboard block or any dynamic-keymap offsets.
 * The custom EEPROM backend protects it with the whole-image journal CRC. */
#    if EECONFIG_USER_DATA_SIZE != 0
#        error "Dev9 brightness policy requires the existing unused QMK user word"
#    endif
#    define MY2K_BRIGHTNESS_POLICY_V9 0x3942524du /* MRB9 */
static void maximum_startup_brightness(my2k_amk_rgb_runtime *rgb) {
    for (unsigned group = 0; group < 8; group++) {
        rgb->groups[group].color.b = 255; /* canonical HSV value, not blue PWM */
        rgb->brightness[group]     = 255;
    }
    for (unsigned led = 0; led < 128; led++)
        if (rgb->rgb.config[led].flags & 1u)
            rgb->rgb.config[led].v = 255;
    for (unsigned led = 0; led < 6; led++)
        if (rgb->indicators[led].flags & 1u)
            rgb->indicators[led].v = 255;
    rgb->configuration_revision++;
    rgb->dirty = true;
}
#endif
void my2k_main_rgb_restore(my2k_main_rgb_settings *state, my2k_amk_rgb_runtime *rgb) {
    if (!state || !rgb)
        return;
#if MY2K_DEV9_PROFILE
    bool restored = false;
#endif
    if (eeconfig_is_kb_datablock_valid() &&
        eeconfig_read_kb_datablock(state->bytes, 0, sizeof state->bytes) == sizeof state->bytes) {
#if MY2K_DEV9_PROFILE
        restored = my2k_rgb_settings_import(rgb, state->bytes, sizeof state->bytes);
#else
        (void)my2k_rgb_settings_import(rgb, state->bytes, sizeof state->bytes);
#endif
    }
#if MY2K_DEV9_PROFILE
    state->brightness_policy_pending =
#    if MY2K_DEV16_PROFILE
        (eeconfig_read_user() & ~MY2K_PERFORMANCE_BIT) != MY2K_BRIGHTNESS_POLICY_V9;
#    else
        eeconfig_read_user() != MY2K_BRIGHTNESS_POLICY_V9;
#    endif
    if (state->brightness_policy_pending || !restored)
        maximum_startup_brightness(rgb);
#endif
    state->saved_revision = 0;
#if MY2K_LOCK_LIGHTS_PROFILE
    state->restored = restored;
#endif
}
void my2k_main_rgb_save_task(my2k_main_rgb_settings *state, const my2k_amk_rgb_runtime *rgb) {
    if (!state || !rgb || state->saved_revision == rgb->configuration_revision)
        return;
    if (my2k_rgb_settings_export(rgb, state->bytes, sizeof state->bytes) &&
        eeconfig_update_kb_datablock(state->bytes, 0, sizeof state->bytes) == sizeof state->bytes) {
#if MY2K_DEV9_PROFILE
        /* Publish the policy marker only after the complete CRC-bearing RGB
         * image is accepted. Both writes share the existing deferred snapshot;
         * a cut before durability re-applies migration from the old snapshot. */
        if (state->brightness_policy_pending) {
            uint32_t marker = MY2K_BRIGHTNESS_POLICY_V9;
#    if MY2K_DEV16_PROFILE
            /* An invalid old marker has no valid performance preference.
             * Start Standard; valid markers (including bit31) never migrate. */
#    endif
            eeconfig_update_user(marker);
            if (eeconfig_read_user() != marker)
                return;
            state->brightness_policy_pending = false;
        }
#endif
        state->saved_revision = rgb->configuration_revision;
    }
}
