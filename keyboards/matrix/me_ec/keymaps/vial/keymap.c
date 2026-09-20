#include QMK_KEYBOARD_H
#include "ch.h"
#include "hal.h"
#include "rgblight.h"




enum layer_names {
    _BASE,
    _FN
};



const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(

         KC_ESC,            KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,  KC_F10,  KC_F11,  KC_F12,          KC_PSCR, KC_SCRL, KC_PAUS,

         KC_GRV,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINS,  KC_EQL, KC_BSPC,           KC_INS, KC_HOME, KC_PGUP,
         KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P, KC_LBRC, KC_RBRC, KC_BSLS,           KC_DEL,  KC_END, KC_PGDN,
        KC_CAPS,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT,           KC_ENT,
        KC_LSFT,             KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH,          KC_RSFT,                     KC_UP,
        KC_LCTL, MO(_FN), KC_LALT,                             KC_SPC,                                     KC_RALT, KC_RGUI, KC_RCTL,          KC_LEFT, KC_DOWN, KC_RGHT),

    [_FN] = LAYOUT(

        QK_BOOT,          UG_PREV, UG_NEXT, UG_HUEU, UG_HUED, UG_SATU, UG_SATD, UG_VALU, UG_VALD, UG_SPDU, UG_SPDD, _______, _______,          _______, _______, _______,

        _______,   KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,  KC_F10,  KC_F11,  KC_F12,  KC_DEL,          _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______, _______, _______,
        KC_LCTL, _______, _______, _______, _______, _______, _______, _______, _______, _______, KC_VOLD, KC_VOLU,          _______,
        _______,          _______, _______, _______, _______, _______, _______, _______, KC_MPRV, KC_MNXT, KC_MPLY,          _______,                   _______,
        KC_LGUI, _______, _______,                            _______,                                     _______, _______, _______,          _______, _______, _______),

};

//——— RGBLIGHT Lighting Layers ——–
#define RGBLAYER_CAPS 0
#define RGBLAYER_FN   1

// Caps Lock layer: white on all LEDs
const rgblight_segment_t PROGMEM capslock_layer[] = RGBLIGHT_LAYER_SEGMENTS(
    { 0, RGBLIGHT_LED_COUNT, HSV_WHITE }
);

// _FN layer: cyan on all LEDs
const rgblight_segment_t PROGMEM fn_layer[] = RGBLIGHT_LAYER_SEGMENTS(
    { 0, RGBLIGHT_LED_COUNT, HSV_CYAN }
);

// Lighting layers list (later overrides earlier)
const rgblight_segment_t* const PROGMEM rgb_layers[] = RGBLIGHT_LAYERS_LIST(
    capslock_layer,
    fn_layer
);

// Initialize RGBLIGHT and attach layers
void keyboard_post_init_user(void) {
    rgblight_enable();             // turn on lights
    rgblight_layers = rgb_layers;  // enable our layers
}

// Sync Caps Lock key state to lighting
bool led_update_user(led_t led_state) {
    rgblight_set_layer_state(RGBLAYER_CAPS, led_state.caps_lock);
    return true;
}

// Sync _FN layer state to lighting
layer_state_t layer_state_set_user(layer_state_t state) {
    rgblight_set_layer_state(RGBLAYER_FN, layer_state_cmp(state, _FN));
    return state;
}


