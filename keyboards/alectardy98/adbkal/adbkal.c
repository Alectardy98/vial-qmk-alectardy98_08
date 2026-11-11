#include QMK_KEYBOARD_H
#include "display.h"
#include "timer.h"

static bool screen_button_down;
static bool caps_target, caps_pending, caps_down, caps_waiting, caps_sent_target;
static uint32_t caps_timer;

static void caps_lock_task(void) {
    uint32_t now = timer_read32();
    if (caps_down) {
        if (now - caps_timer >= TAP_HOLD_CAPS_DELAY) {
            unregister_code(KC_CAPS);
            caps_down = false;
            caps_timer = now;
        }
        return;
    }

    // Wait for host feedback before handling a switch reversal queued during
    // a pulse. Bound the wait in case the host never acknowledges the toggle.
    if (caps_waiting) {
        if (host_keyboard_led_state().caps_lock != caps_sent_target && now - caps_timer < 500) return;
        caps_waiting = false;
    }
    if (!caps_pending) return;
    caps_pending = false;
    if (host_keyboard_led_state().caps_lock == caps_target) return;

    // Both locking-switch edges send the same timed pulse, without stopping
    // matrix scanning while the Caps key is held in the USB report.
    register_code(KC_CAPS);
    caps_sent_target = caps_target;
    caps_down = caps_waiting = true;
    caps_timer = now;
}

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
    if (keycode == KC_LCAP) {
        if (!process_record_user(keycode, record)) return false;
        caps_target = record->event.pressed;
        caps_pending = true;
        return false;
    }
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
    caps_lock_task();
    adbk_display_task();
    housekeeping_task_user();
}
