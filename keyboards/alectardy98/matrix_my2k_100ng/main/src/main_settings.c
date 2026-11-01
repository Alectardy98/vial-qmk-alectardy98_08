/* SPDX-License-Identifier: GPL-2.0-or-later */
#include QMK_KEYBOARD_H
#include "main_settings.h"
#include "keycode_config.h"
#include "vial.h"
#include "eeconfig.h"
#if MY2K_DEV16_PROFILE
#include "main_performance_mode.h"
#endif
#if MY2K_STOCK_PARITY_PROFILE
#include "my2k_amk_rgb_runtime.h"
static my2k_amk_rgb_runtime *active;
#endif
void my2k_main_settings_bind(void *context){
#if MY2K_STOCK_PARITY_PROFILE
    active=context;
#else
    (void)context;
#endif
}
uint8_t my2k_main_debounce(bool pressed){
#if MY2K_STOCK_PARITY_PROFILE
    if(active && active->settings)return pressed?active->debounce_press:active->debounce_release;
#else
    (void)pressed;
#endif
    return DEBOUNCE;
}
bool my2k_main_scalar(void *context, uint8_t command, uint8_t *value) {
    (void)context;
    if (!value) return false;
#if MY2K_STOCK_PARITY_PROFILE
    my2k_amk_rgb_runtime *r=context;active=r;
    if(r && (command==9 || command==11)){*value=command==9?r->debounce_press:r->debounce_release;return true;}
#endif
    switch (command) {
        case 7:
#if MY2K_DEV16_PROFILE
            *value = my2k_performance_usb_speed()==2?3:0;
#else
            *value = 0;
#endif
            return true; /* Actual enumerated endpoint interval, not scan rate. */
        case 9:
        case 11:
            *value = DEBOUNCE;
            return true;
        case 13:
            *value = keymap_config.nkro ? 1 : 0;
            return true;
        default:
            return false;
    }
}
bool my2k_main_scalar_set(void *context, uint8_t command, uint8_t value) {
    (void)context;
    if(!vial_unlocked)return false;
#if MY2K_STOCK_PARITY_PROFILE
    my2k_amk_rgb_runtime *r=context;active=r;
    if(r && (command==10 || command==12)){
        if(value>10)return false;
        if(command==10)r->debounce_press=value;else r->debounce_release=value;
        r->dirty=true;r->configuration_revision++;return true;
    }
#endif
    if(command != 14 || value > 1)return false;
    if (keymap_config.nkro != (value != 0)) {
        /* QMK process_magic uses this same release-before-format-switch rule.
         * This is a host report release before mutating the report format. */
        clear_keyboard();
        keymap_config.nkro = value != 0;
        /* Standard QMK RAM EEPROM lifecycle; no direct flash access. */
        eeconfig_update_keymap(&keymap_config);
    }
    return true;
}
