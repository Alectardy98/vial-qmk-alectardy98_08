/* Copyright 2021 djvs
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
static bool lock_lights_enabled = true;

enum layer_names {
    _BASE,
    _NUM,
    _FN
};

bool rgb_matrix_indicators_user(void) {   //RGB Lock Light Control
    if (!lock_lights_enabled) {
        return false;  // Skip setting LED colors if disabled
    }
    if (host_keyboard_led_state().caps_lock) {
        rgb_matrix_set_color(1, 255, 0, 0);  // Red for Caps Lock
    } else {
        rgb_matrix_set_color(1, 0, 0, 0);
    }
    if (layer_state_is(_FN)) {               // Force Scroll Lock LED On when in FN layer
        rgb_matrix_set_color(0, 0, 0, 255);  // Blue for Scroll Lock
    } else {
        rgb_matrix_set_color(0, 0, 0, 0);
    }
    if (layer_state_is(_NUM)) {              // Force Num Lock LED On when in NUM layer
        rgb_matrix_set_color(2, 0, 255, 0);  // Green for Num Lock
    } else {
        rgb_matrix_set_color(2, 0, 0, 0);
    }
    return false;
}
enum blender_keycode {
    VDRT = QK_KB_0,
    VDLT,                   //Desktop Left "set to move left a space on mac"
    VDUP,                   //V-Desktop Up "set to mission controll on mac"
    VDDN,                   //Min All "set to show desktop on mac"
    EXPL,                   //New Explorer On Windows "set to show launchpad on mac"
    SNIP,                   //Snip Tool Windows "set to save picture of selected area on mac"
    QUIT,                   //Close App
    RGBT,
};

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {             // Code for macros
        switch (keycode) {
            case VDRT:
                SEND_STRING(SS_LGUI(SS_DOWN(X_LCTL) SS_TAP(X_RIGHT) SS_UP(X_LCTL)));
                break;
            case VDLT:
                SEND_STRING(SS_LGUI(SS_DOWN(X_LCTL) SS_TAP(X_LEFT) SS_UP(X_LCTL)));
                break;
            case VDUP:
                SEND_STRING(SS_LGUI(SS_TAP(X_TAB) SS_TAP(X_T)));
                break;
            case VDDN:
                SEND_STRING(SS_LGUI("d"));
                break;
            case EXPL:
                SEND_STRING(SS_LGUI("e"));
                break;
            case SNIP:
                SEND_STRING(SS_LGUI(SS_LSFT("s")));
                break;
            case QUIT:
                SEND_STRING(SS_LALT(SS_TAP(X_F4)));     // Windows
                SEND_STRING(SS_LCTL(SS_TAP(X_W)));      // Mac
                break;
            case RGBT:
                lock_lights_enabled = !lock_lights_enabled;
                rgb_matrix_indicators_user();  // Force update
                return false;  // Skip further processing
        }
    }
    return true;
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    
[_BASE] = LAYOUT(
KC_ESC,   KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,  KC_F10,  KC_F11,  KC_F12,             KC_PSCR, KC_SCRL, KC_PAUS,                               CK_TOGG,
KC_GRV,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINS,  KC_EQL, KC_BSPC,     KC_INS, KC_HOME, KC_PGUP,   TG(_NUM), KC_PSLS, KC_PAST, KC_PEQL,
KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P, KC_LBRC, KC_RBRC, KC_BSLS,     KC_DEL,  KC_END, KC_PGDN,       KC_7,    KC_8,    KC_9, KC_PMNS,
KC_LCAP,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT,  KC_ENT,                                              KC_4,    KC_5,    KC_6, KC_PPLS,
KC_LSFT,    KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH, KC_RSFT,                                 KC_UP,                KC_1,    KC_2,    KC_3, _______,
KC_LCTL, MO(_FN), KC_LALT,                    KC_SPC,                            KC_RALT, KC_RGUI, KC_RCTL,                      KC_LEFT, KC_DOWN, KC_RGHT,       KC_0, _______,  KC_DOT,  KC_PENT),

[_NUM] = LAYOUT(
_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,             _______, _______, _______,                               _______,
_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,    _______, _______, _______,    _______, _______, _______, _______,
_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,    _______, _______, _______,    KC_HOME,   KC_UP, KC_PGUP, _______,
_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                                           KC_LEFT, _______, KC_RGHT, _______,
_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                               _______,              KC_END,  KC_END, KC_PGDN, _______,
_______, _______, _______,                   _______,                            _______, _______, _______,                      _______, _______, _______,    KC_INS, _______,   KC_DEL, _______),
    
[_FN] = LAYOUT(
QK_BOOT, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,             _______, _______, _______,                                  RGBT,
_______,   KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12, KC_DEL,    _______, _______, _______,    _______, _______, _______, _______,
  SNIP,  _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,    _______, _______, _______,    _______,    VDUP, _______, CK_DOWN,
KC_LCTL, C(KC_A), C(KC_S), C(KC_D), C(KC_F), _______, _______, _______, _______, _______, KC_VOLD, KC_VOLU, _______,                                              VDLT,    QUIT,    VDRT,   CK_UP,
_______, C(KC_Z), C(KC_X), C(KC_C), C(KC_V), _______, _______, _______, KC_MPRV, KC_MNXT, KC_MPLY, _______,                                  VDUP,             _______,    VDDN, _______, _______,
KC_LGUI, _______, _______,                   _______,                            _______, _______, _______,                         VDLT,    VDDN,    VDRT,    _______, _______, _______, _______),
    
};
