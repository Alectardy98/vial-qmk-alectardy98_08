#include QMK_KEYBOARD_H
#include "display.h"

static bool screen_button_down;

bool pre_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    // Reserve the physical power/display switch on every layer, including
    // existing Vial EEPROM maps that still assign KC_PWR to this position.
    if (record->event.key.row == 10 && record->event.key.col == 5) {
        if (record->event.pressed && !screen_button_down) adbk_display_next_mode();
        screen_button_down = record->event.pressed;
        return false;
    }
    if (record->event.pressed && wpm_keycode(keycode)) adbk_display_note_key();
    return pre_process_record_user(keycode, record);
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (keycode == ADBK_SCREEN) {
        if (record->event.pressed) adbk_display_next_mode();
        return false;
    }
    return process_record_user(keycode, record);
}

void keyboard_post_init_kb(void) {
    adbk_display_init();
    keyboard_post_init_user();
}

void housekeeping_task_kb(void) {
    adbk_display_task();
    housekeeping_task_user();
}
