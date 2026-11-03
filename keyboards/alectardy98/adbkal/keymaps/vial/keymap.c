/* Copyright 2026 Alectardy98
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

enum layers {
    _BASE,
    _CAPS,
    _FN
};

// Keys follow their physical positions: knob/screen mode, then the five keyboard rows.
// Both Shift switches share one input, represented by the left Shift position.
// Return is listed on the Tab row; keypad Enter is listed on the Shift row.
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
          KC_MUTE,                                       SC_MODE,
      KC_ESC,     KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,  KC_MINS,   KC_EQL,  KC_BSPC,                 TG(_FN),  KC_PEQL,  KC_PSLS,  KC_PAST,
      KC_TAB,          KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,     KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,  KC_LBRC,  KC_RBRC,   KC_ENT,              KC_P7,    KC_P8,    KC_P9,  KC_PPLS,
     KC_LCTL,             KC_A,     KC_S,     KC_D,     KC_F,     KC_G,     KC_H,     KC_J,     KC_K,     KC_L,  KC_SCLN,  KC_QUOT,                               KC_P4,    KC_P5,    KC_P6,  KC_PMNS,
     KC_LSFT,                 KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,  KC_COMM,   KC_DOT,  KC_SLSH,                                     KC_P1,    KC_P2,    KC_P3,  KC_PENT,
     KC_LCAP,  KC_LALT,  KC_LGUI,           KC_GRV,   KC_SPC,                                       KC_BSLS,  KC_LEFT,  KC_RGHT,  KC_DOWN,    KC_UP,              KC_P0,            KC_PDOT
    ),

    [_CAPS] = LAYOUT(
          _______,                                     _______,
     _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,                 _______,  _______,  _______,  _______,
     _______,       S(KC_Q),  S(KC_W),  S(KC_E),  S(KC_R),  S(KC_T),  S(KC_Y),  S(KC_U),  S(KC_I),  S(KC_O),  S(KC_P),  _______,  _______,  _______,            _______,  _______,  _______,  _______,
     _______,          S(KC_A),  S(KC_S),  S(KC_D),  S(KC_F),  S(KC_G),  S(KC_H),  S(KC_J),  S(KC_K),  S(KC_L),  _______,  _______,                             _______,  _______,  _______,  _______,
     _______,              S(KC_Z),  S(KC_X),  S(KC_C),  S(KC_V),  S(KC_B),  S(KC_N),  S(KC_M),  _______,  _______,  _______,                                   _______,  _______,  _______,  _______,
     _______,  _______,  _______,          _______,  _______,                                       _______,  _______,  _______,  _______,  _______,            _______,            _______
    ),

    [_FN] = LAYOUT(
          _______,                                     _______,
     _______,    KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,   KC_F10,  KC_VOLD,  KC_VOLU,   KC_DEL,                 _______,  _______,  _______,  _______,
     _______,       _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,
     _______,          _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,                             _______,  _______,  _______,  _______,
     _______,              _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,                                   _______,  _______,  _______,  _______,
     _______,  _______,  _______,          _______,  _______,                                       _______,  _______,  _______,  _______,  _______,            _______,            _______
    ),
};

#if defined(ENCODER_MAP_ENABLE)
// Counterclockwise / clockwise knob bindings for each layer.
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
    [_BASE] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [_CAPS] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [_FN]   = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
};
#endif
