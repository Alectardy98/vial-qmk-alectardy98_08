/* Copyright 2022 Alectardy98
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include QMK_KEYBOARD_H

enum custom_keycodes {
    EC_NOIS = QK_KB_0,
    EC_BOTM,
    EC_PRNT,
    LK_SONG,
};

_Static_assert(EC_NOIS == (int)CLASS0413_EC_NOISE, "EC Noise keycode ABI");
_Static_assert(EC_BOTM == (int)CLASS0413_EC_BOTTOM_START, "EC Bottom keycode ABI");
_Static_assert(EC_PRNT == (int)CLASS0413_EC_CAL_PRINT, "EC Print keycode ABI");

_Static_assert(LK_SONG == (int)CLASS0413_LK_SONG, "Lock song keycode ABI");

enum _layer {
    _BASE,
    _FN,
    _LAYER2,
    _LAYER3,
};

/* LAYOUT follows the verified 91-position electrical order. Vial retains
 * the physical geometry and alternative arrangements. Unscanned cells are KC_NO.
 */
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
          KC_F1,   KC_F2,      KC_ESC,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINS,  KC_EQL, KC_BSPC, KC_BSPC,           KC_NUM, KC_PSLS, KC_PAST, KC_PMNS,
          KC_F3,   KC_F4,      KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P, KC_LBRC, KC_RBRC, KC_BSLS,                     KC_P7, KC_P8, KC_P9, KC_PPLS,
          KC_F5,   KC_F6,     KC_CAPS,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT, KC_BSLS,  KC_ENT,                     KC_P4, KC_P5, KC_P6,
          KC_F7,   KC_F8,     KC_LSFT, KC_BSLS,    KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH, KC_RSFT,  KC_DEL,                     KC_P1, KC_P2, KC_P3,
          KC_F9,  KC_F10,     KC_LCTL, KC_LGUI, KC_LALT,                             KC_SPC,                            MO(_FN), KC_RGUI, KC_RCTL,                              KC_P0, KC_PDOT, KC_PENT
    ),
    [_FN] = LAYOUT(
        EC_NOIS, EC_BOTM,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,     _______, _______, _______, _______,
        EC_PRNT, QK_BOOT,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                  _______, _______, _______, _______,
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                   _______, _______, _______,
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                   _______, _______, _______,
        _______, _______,     _______, _______, _______,                            _______,                            _______, _______, _______,                            _______, _______, _______
    ),
    [_LAYER2] = LAYOUT(
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______,
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                   _______, _______, _______, _______,
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                   _______, _______, _______,
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                   _______, _______, _______,
        _______, _______,     _______, _______, _______,                            _______,                            _______, _______, _______,                            _______, _______, _______
    ),
    [_LAYER3] = LAYOUT(
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______,
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                   _______, _______, _______, _______,
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                   _______, _______, _______,
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                   _______, _______, _______,
        _______, _______,     _______, _______, _______,                            _______,                            _______, _______, _______,                            _______, _______, _______
    ),
};

/* Add future keymap macros here. EC controls deliberately pass through to
 * process_record_kb(), which owns their release gate and calibration logic. */
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        switch (keycode) {
            default:
                break;
        }
    }
    return true;
}
