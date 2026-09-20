/* Copyright 2022 Alectardy98
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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


enum layer_names {  //Define Layers
    _BASE,
    _NUM
};

enum blender_keycode {
    TEST = QK_KB_0,
    TZRO,                   //Desktop Right "set to move right a space on mac"
};
    
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {             // Code for macros
        switch (keycode) {
            case TEST:
                SEND_STRING("TEST");
                break;
            case TZRO:
                SEND_STRING("000");
                break;
        }
    }

#ifdef CONSOLE_ENABLE // Console Debug
    uprintf("KL: kc: 0x%04X, col: %u, row: %u, pressed: %1u, time: %u, interrupt: %1u, count: %u\n",
            keycode, record->event.key.col, record->event.key.row, record->event.pressed, record->event.time,
            record->tap.interrupted, record->tap.count);
#endif

    return true;
}
        
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    [_BASE] = LAYOUT(
                    MO(_NUM),
                     KC_PAST, KC_PSLS, KC_PMNS, KC_PPLS,
                        KC_7,    KC_8,    KC_9, KC_PEQL,
                        KC_4,    KC_5,    KC_6, KC_PDOT,
                        KC_1,    KC_2,    KC_3, KC_COMM,
                        KC_0,    TZRO,  KC_ENT
        ),
    [_NUM] = LAYOUT(
                   _______,
                    KC_ESC, _______,  KC_INS,  KC_DEL,
                   _______,   KC_UP, _______, _______,
                   KC_LEFT, _______, KC_RGHT, _______,
                   _______, KC_DOWN, _______, _______,
                   _______, _______, _______
        )
};
