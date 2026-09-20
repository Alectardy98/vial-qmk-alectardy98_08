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

enum _layer {
  _BASE,
  _FN
};
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

[_BASE] = LAYOUT(
      KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,  KC_F10,  KC_F11,  KC_F12,  KC_F13,  KC_F14,      KC_INS,  KC_DEL,                  KC_F17,  KC_F18,  KC_F19,  KC_F20,
     KC_GRV,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINS,  KC_EQL, KC_BSPC,      KC_INS, KC_HOME, KC_PGUP,         KC_NUM, KC_PSLS, KC_PAST, KC_PMNS,
     KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P, KC_LBRC, KC_RBRC,  KC_ENT,      KC_DEL,  KC_END, KC_PGDN,          KC_P7,   KC_P8,   KC_P9, KC_PPLS,
    KC_LCTL, KC_CAPS,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT, KC_BSLS,                KC_UP,                   KC_P4,   KC_P5,   KC_P6, _______,
    KC_LSFT, KC_LSFT,    KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH, KC_RSFT,              KC_LEFT, KC_DOWN, KC_RGHT,          KC_P1,   KC_P2,   KC_P3, KC_PENT,
                      MO(_FN),                                      KC_SPC,                                                                                               KC_P0,          KC_PDOT),

    

    

[_FN] = LAYOUT(
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,     _______, _______,                 _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,     _______, _______, _______,        _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,     _______, _______, _______,        _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,              _______,                 _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,              _______, _______, _______,        _______, _______, _______, _______,
                      _______,                                     _______,                                                                                             _______,          _______),
};


const uint8_t music_map[MATRIX_ROWS][MATRIX_COLS] = LAYOUT(
    56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69,     0, 0,           0, 0, 0, 0,
    42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,     0, 0, 0,        0, 0, 0, 0,
    28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,     0, 0, 0,        0, 0, 0, 0,
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,        0,           0, 0, 0, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,                  0, 0, 0,        0, 0, 0, 0,
          0,             0,                                                     0,    0
);
