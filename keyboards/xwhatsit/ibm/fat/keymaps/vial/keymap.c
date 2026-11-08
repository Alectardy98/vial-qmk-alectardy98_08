/*
 Note: This config allows for the use of num lock states in both Mac OS and Windows will relativly seamless functionality. In order to make this happen a couple of consessions had to be made. The function matrix_scan_user(void) is always running on a constant loop and will result in preformance loss that can be midigated by disableing the code. I personaly have not noticed anything wrong but your results may varry. Also it is important to note that if using 2 keyboards on the same windows computer, by pushing num lock key on a second keyboard from the on state to the off state, this keyboard will become out of sinc with the host OS and will be stuck in the num lock state untill num lock is pressed again. Other then this rare occurance I do not belive any other issues occure.
 */

#include QMK_KEYBOARD_H


// Defines names for use in layer keycodes and the keymap
enum layer_names {
    _BASE,
    _NUM,
    _FN,
};





const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
          KC_F1,   KC_F2,    KC_GRV,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINS,  KC_EQL, _______, KC_BSPC,          KC_NUM, KC_PSLS, KC_PAST, KC_PMNS,
          KC_F3,   KC_F4,    KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P, KC_LBRC, KC_RBRC,          KC_BSLS,           KC_P7,   KC_P8,   KC_P9, KC_PPLS,
          KC_F5,   KC_F6,   KC_CAPS,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT, _______,           KC_ENT,           KC_P4,   KC_P5,   KC_P6, _______,
          KC_F7,   KC_F8,   KC_LSFT, _______,    KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH, _______,          KC_RSFT,           KC_P1,   KC_P2,   KC_P3, KC_PENT,
          KC_F9,  KC_F10,   KC_LCTL,          KC_LALT,                             KC_SPC,                                              KC_RALT,          KC_RCTL,         _______,   KC_P0, KC_PDOT, _______
    ),
    [_NUM] = LAYOUT(
        _______, _______,   _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,         _______, _______, _______, _______,
        _______, _______,   _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______,         _______, _______, _______, _______,
        _______, _______,   _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______,         _______, _______, _______, _______,
        _______, _______,   _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______,         _______, _______, _______, _______,
        _______, _______,   _______,          _______,                            _______,                                              _______,          _______,         _______, _______, _______, _______
    ),
    [_FN] = LAYOUT(
        _______, _______,   _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,         _______, _______, _______, _______,
        _______, _______,   _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______,         _______, _______, _______, _______,
        _______, _______,   _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______,         _______, _______, _______, _______,
                   _______, _______,   _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______,         _______, _______, _______, _______,
        _______, _______,   _______,          _______,                            _______,                                              _______,          _______,         _______, _______, _______, _______
    ),
    
};


