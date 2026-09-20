/* Copyright 2022 Alectardy98
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by			
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
#include "quantum.h"   // for host_keyboard_led_state(), hsv_to_rgb(), etc.
#include "timer.h"     // for timer_read()

enum _layer {
  _BASE,
  _FN
};

#define LED_NUM   0   // matrix [1,0]
#define LED_CAPS  1   // matrix [1,1]
#define LED_FN    2   // matrix [1,2]

// Helper: off
static inline void rgb_off(uint8_t i) { rgb_matrix_set_color(i, 0, 0, 0); }

bool rgb_matrix_indicators_user(void) {
    led_t host = host_keyboard_led_state();

    // scale brightness to 1/6
    uint8_t scale = 128;   // 255/6 ≈ 42

    // CAPS → red (now on LED_CAPS index 1)
    if (host.caps_lock) {
        rgb_matrix_set_color(LED_CAPS, scale, 0, 0);
    } else {
        rgb_off(LED_CAPS);
    }

    // NUM → blue (now on LED_NUM index 0)
    if (host.num_lock) {
        rgb_matrix_set_color(LED_NUM, 0, 0, scale);
    } else {
        rgb_off(LED_NUM);
    }

    // FN layer → rainbow on LED_FN, starting at yellow
    if (layer_state_is(_FN)) {
        uint16_t t = timer_read();
        uint8_t hue = ((t >> 3) + 42) & 0xFF; // shift hue to start at yellow
        HSV hsv = { hue, 255, scale };        // scaled brightness
        RGB rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(LED_FN, rgb.r, rgb.g, rgb.b);
    } else {
        rgb_off(LED_FN);
    }

    return true;
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    [_BASE] = LAYOUT(
     KC_ESC, _______,   KC_F1,   KC_F2,   KC_F3,    KC_F4,   KC_F5,   KC_F6,          KC_INS, KC_HOME, KC_PGUP, KC_PSCR,          KC_PAUS,           KC_NUM, KC_PSLS, KC_PAST,  KC_PMNS,
    _______, _______,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,          KC_DEL,  KC_END, KC_PGDN, KC_SCRL,          _______,            KC_P7,   KC_P8,   KC_P9,  KC_PPLS,
                                                                                                                                                      KC_P4,   KC_P5,   KC_P6,  _______,
     KC_GRV,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINS, KC_EQL, KC_BSPC,  _______,            KC_P1,   KC_P2,   KC_P3,  KC_PEQL,
     KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P, KC_LBRC,KC_RBRC, KC_BSLS,                      KC_P0, _______, KC_PDOT,  _______,
    KC_CAPS,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT, KC_ENT,
    KC_LSFT, _______,    KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH,KC_RSFT,             KC_UP,
    KC_LCTL, KC_LGUI, KC_LALT,                    KC_SPC,                   KC_RALT, KC_RGUI, MO(_FN), KC_RCTL,          KC_LEFT, KC_DOWN, KC_RGHT,             MS_BTN1,   MS_BTN2),


[_FN] = LAYOUT(
    _______, _______, _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______,          _______,          _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______,          _______,          _______, _______, _______, _______,
                                                                                                                                                    _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                   _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______,
    _______, _______, _______,                   _______,                   _______, _______, _______, _______,          _______, _______, _______,          _______, _______)
};


const uint8_t music_map[MATRIX_ROWS][MATRIX_COLS] = LAYOUT(
    0, 0, 0, 0, 0, 0, 0, 0,          0, 0, 0, 0,          0,          0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,          0, 0, 0, 0,          0,          0, 0, 0, 0,
                                                                      0, 0, 0, 0,
    0, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 0, 0,          0, 0, 0, 0,
    0, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 0,             0, 0, 0, 0,
    0, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 0,
    0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,          0,
    0, 0, 0,          0,         0, 0, 0, 0,       0, 0, 0,              0, 0
);
