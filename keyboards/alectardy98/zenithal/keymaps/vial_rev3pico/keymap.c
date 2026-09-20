/* Copyright 2022 Alectardy98
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H

bool beep_enabled = true;  // Beep mode starts enabled

void matrix_init_user(void) {
    beep_enabled = true;  // Ensure beep is enabled on startup
}

enum layer_names {  
    _BASE,
    _NUM,
    _FN
};



enum blender_keycode {
    BEEP = QK_KB_0,  // Custom key for toggling beep
};


bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        switch (keycode) {
            case BEEP:  // Toggle beeper mode
                beep_enabled = !beep_enabled;
                return false;  // Skip default key processing
        }
        // Beep on keypress if enabled
        if (beep_enabled) {
            writePinHigh(GP17);
            wait_ms(2);  // Delay in milliseconds
            writePinLow(GP17);
        }
    }

#ifdef CONSOLE_ENABLE  // Debugging
    uprintf("KL: kc: 0x%04X, col: %u, row: %u, pressed: %1u, time: %u, interrupt: %1u, count: %u\n", 
        keycode, record->event.key.col, record->event.key.row, record->event.pressed, 
        record->event.time, record->tap.interrupted, record->tap.count);
#endif
    return true;
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    [_BASE] = LAYOUT(
          KC_F1,    KC_F2,    KC_ESC,     KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINS,  KC_EQL, KC_BSPC,  KC_GRV,TG(_NUM), _______, CK_TOGG,
          KC_F3,    KC_F4,    KC_TAB,     KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P, KC_LBRC, KC_RBRC,  KC_ENT, KC_HOME,   KC_UP, KC_PGUP, KC_PMNS,
          KC_F5,    KC_F6,    KC_LCTL,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT, _______,          KC_LEFT, _______, KC_RGHT, KC_PPLS,
          KC_F7,    KC_F8,    KC_LSFT, _______,    KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH, KC_RSFT, KC_BSLS,  KC_END, KC_DOWN, KC_PGDN, KC_PENT,
          KC_F9,   KC_F10,    KC_LALT, MO(_FN), _______,                   _______,  KC_SPC,                            _______, KC_CAPS, _______,  KC_INS, _______,  KC_DEL, _______, _______
        ),
    [_NUM] = LAYOUT(
        _______, _______,    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______,    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,    KC_7,    KC_8,    KC_9, _______,
        _______, _______,    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,             KC_4,    KC_5,    KC_6, _______,
        _______, _______,    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,    KC_1,    KC_2,    KC_3, _______,
        _______, _______,    _______, _______, _______,                   _______, _______,                            _______, _______, _______,    KC_0, _______, KC_PDOT, _______, _______
        ),
    [_FN] = LAYOUT(
        _______, _______,    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______,    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______,    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______,
        _______, _______,    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______,    _______, _______, _______,                   _______, _______,                            _______, _______, _______, _______, _______, _______, _______, _______
        ),
};

const uint8_t music_map[MATRIX_ROWS][MATRIX_COLS] = LAYOUT(
        74, 75,    76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93,
        54, 55,    56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73,
        35, 36,    37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,          50, 51, 52, 53,
        15, 16,    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
        0, 1,    2, 3, 4,                   5, 6,                            7, 8, 9, 10, 11, 12, 13, 14
);
