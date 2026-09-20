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
                        KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,  KC_F10,
     KC_GRV,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINS,  KC_EQL, KC_BSPC, _______,      KC_NUM, KC_SCRL,  KC_ESC, KC_PMNS,
     KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P, KC_LBRC, KC_RBRC, _______,                KC_P7,   KC_P8,   KC_P9, KC_PPLS,
    KC_LCTL,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT,  KC_ENT,                         KC_P4,   KC_P5,   KC_P6, _______,
    KC_LSFT,    KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH, KC_RSFT, KC_BSLS,                         KC_P1,   KC_P2,   KC_P3, KC_PENT,
    KC_LCTL, KC_LALT,                             KC_SPC,                                     KC_RALT, KC_RCTL,                                  KC_P0, _______, KC_PDOT, _______),

    

    

[_FN] = LAYOUT(
                      _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,     _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,              _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                       _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                       _______, _______, _______, _______,
    _______, _______,                            _______,                                     _______, _______,                                _______, _______, _______, _______),
};


const uint8_t music_map[MATRIX_ROWS][MATRIX_COLS] = LAYOUT(
    51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,     0, 0, 0, 0,
    24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 36,         0, 0, 0, 0,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 23,             0, 0, 0, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 11, 11,                      0, 0, 0, 0,
    0, 0,             0,             0, 0,                          0, 0, 0, 0
);
