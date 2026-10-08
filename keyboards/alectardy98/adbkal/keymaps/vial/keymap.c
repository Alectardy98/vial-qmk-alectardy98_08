#include QMK_KEYBOARD_H

enum layers { BASE, CAPS, FN };

// Generated from the original ZMK default_transform RC(row,col) map.
// Physical/electrical scan rows are not the same as visible keyboard rows.
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [BASE] = {
        { KC_ESC, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_9 },
        { KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_7, KC_8 },
        { KC_TAB, KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U },
        { KC_QUOT, KC_SCLN, KC_ENT, KC_RBRC, KC_LBRC, KC_P, KC_O, KC_I },
        { KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K },
        { KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M, KC_COMM },
        { KC_SPC, KC_LEFT, KC_BSLS, KC_GRV, KC_PDOT, KC_DOWN, KC_P0, KC_RGHT },
        { KC_PAST, KC_PSLS, KC_PEQL, TG(FN), KC_PENT, KC_P3, KC_P2, KC_P1 },
        { KC_PMNS, KC_P9, KC_P8, KC_P7, KC_PPLS, KC_P6, KC_P5, KC_P4 },
        { KC_UP, KC_SLSH, KC_DOT, KC_L, KC_BSPC, KC_EQL, KC_MINS, KC_0 },
        { KC_LCTL, KC_LSFT, KC_LALT, KC_LGUI, KC_MUTE, KC_PWR, KC_NO, KC_NO },
        { KC_LCAP, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO },
    },
    [CAPS] = {
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, S(KC_Q), S(KC_W), S(KC_E), S(KC_R), S(KC_T), S(KC_Y), S(KC_U) },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, S(KC_P), S(KC_O), S(KC_I) },
        { S(KC_A), S(KC_S), S(KC_D), S(KC_F), S(KC_G), S(KC_H), S(KC_J), S(KC_K) },
        { S(KC_Z), S(KC_X), S(KC_C), S(KC_V), S(KC_B), S(KC_N), S(KC_M), KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, S(KC_L), KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
    },
    [FN] = {
        { KC_TRNS, KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F9 },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_F7, KC_F8 },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_DEL, KC_VOLU, KC_VOLD, KC_F10 },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
        { KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS },
    },
};

#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
    [BASE] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [CAPS] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [FN] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
};
#endif
