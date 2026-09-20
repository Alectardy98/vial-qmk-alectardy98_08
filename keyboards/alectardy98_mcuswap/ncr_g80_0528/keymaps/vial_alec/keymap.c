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
  _NUM,
  _FN
};

bool led_update_user(led_t led_state) {
    if (IS_LAYER_ON(_FN) || led_state.scroll_lock) {
        writePinHigh(GP28);  // ON (active-low)
    } else {
        writePinLow(GP28); // OFF
    }

    if (IS_LAYER_ON(_NUM) || led_state.num_lock) {
        writePinHigh(GP26);  // ON (active-low)
    } else {
        writePinLow(GP26); // OFF
    }

    if (led_state.caps_lock) {
        writePinHigh(GP27);  // ON (active-low)
    } else {
        writePinLow(GP27); // OFF
    }

    return false;  // override QMK’s built-in LED logic
}



// Defines the keycodes used by our macros in process_record_user
enum blender_keycode {
    TEST = QK_KB_0,
    VDRT,                   //Desktop Right "set to move right a space on mac"
    VDLT,                   //Desktop Left "set to move left a space on mac"
    VDUP,                   //V-Desktop Up "set to mission controll on mac"
    VDDN,                   //Min All "set to show desktop on mac"
    EXPL,                   //New Explorer On Windows "set to show launchpad on mac"
    SNIP,                   //Snip Tool Windows "set to save picture of selected area on mac"
    COPY,                   //Copy All
    CUTT,                   //Cut All
    PAST,                   //Paste Over
    TASK,                   //Task Manager
    GOGL,                   //Opens Google Chrome, Set in automator on mac to Controll + Alt + G
    CMD,                    //Opens Comand Prompt, Set In automator on mac to Controll + Alt + T
    QUIT,                   //Close App
    DZRO,                   //Double Zero
    ATAB,                   //Alt + Tab "brew install alt-tab on mac"
    LPRC,                   //The "(" Key
    RPRC,                   //The ")" Key
    NUM,                    //Num Lock for both mac and windows
    //Discord Section
    THIS,                   //Macro for ":this:" emoji on discord
    OOF,                    //Macro for ":oof:" emoji on discord
    PIKA,                   //Macro for ":surprised:" emoji on discord
    SCAT,                   //Macro for ":smiley_cat2:" emoji on discord
    FCAT,                   //Macro for ":smiley_cat1:" emoji on discord
    HART,                   //Macro for ":heart:" emoji on discord
    DROL,                   //Macro for ":drooling_face:" emoji on discord
    MONY,                   //Macro for ":smiley_take:" emoji on discord
    FHAT,                   //Macro for ":smiley_fedora:" emoji on discord
    SPIT,                   //Macro for ":smiley_spit:" emoji on discord
    KING,                   //Macro for ":pepe_king:" emoji on discord
    FLEX,                   //Macro for ":muscle:" emoji on discord
    HAHA,                   //Macro for ":smiley_kekw:" emoji on discord
    LCRY,                   //Macro for ":joy:" emoji on discord
    NICE,                   //Macro for ":nice~2:" emoji on discord
    PPOG,                   //Macro for ":pepe_pog:" emoji on discord
    PRAY,                   //Macro for ":pray_tone2:" emoji on discord
    SWET,                   //Macro for ":sweat_smile:" emoji on discord
    COLD,                   //Macro for ":cold_face:" emoji on discord
    THUM,                   //Macro for ":thumbsup:" emoji on discord
};
    
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {             // Code for macros
        switch (keycode) {
            case TEST:
                SEND_STRING("TEST");
                break;
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
            case COPY:
                SEND_STRING(SS_TAP(X_F2) SS_LCTL(SS_TAP(X_A) SS_TAP(X_C)));
                break;
            case CUTT:
                SEND_STRING(SS_TAP(X_F2) SS_LCTL(SS_TAP(X_A) SS_TAP(X_X)));
                break;
            case PAST:
                SEND_STRING(SS_TAP(X_F2) SS_LCTL(SS_TAP(X_V)));
                break;
            case TASK:
                SEND_STRING(SS_LCTL(SS_DOWN(X_RSFT) SS_TAP(X_ESCAPE) SS_UP(X_RSFT)));  // Windows
                SEND_STRING(SS_LCTL(SS_DOWN(X_LALT) SS_TAP(X_ESCAPE) SS_UP(X_LALT)));    // Mac
                break;
            case GOGL:
                SEND_STRING(SS_LCTL(SS_LALT(SS_TAP(X_G))));
                break;
            case CMD:
                SEND_STRING(SS_LCTL(SS_LALT(SS_TAP(X_T))));
                break;
            case QUIT:
                SEND_STRING(SS_LALT(SS_TAP(X_F4)));     // Windows
                SEND_STRING(SS_LCTL(SS_TAP(X_W)));      // Mac
                break;
            case DZRO:
                SEND_STRING("00");
                break;
            case ATAB:
                SEND_STRING(SS_LALT(SS_TAP(X_TAB)));
                break;
            case LPRC:
                SEND_STRING("(");
                break;
            case RPRC:
                SEND_STRING(")");
                break;
            case NUM:
                SEND_STRING(SS_TAP(X_NUM));         // Toggles layer change and presses the num lock button, to allow for Num lock toggle to occur for both Mac OS and Windows
                return true;
                break;
            // Discord Section
            case THIS:
                SEND_STRING(":this:" SS_TAP(X_ENTER));
                break;
            case OOF:
                SEND_STRING(":oof:" SS_TAP(X_ENTER));
                break;
            case PIKA:
                SEND_STRING(":surprised:" SS_TAP(X_ENTER));
                break;
            case SCAT:
                SEND_STRING(":smiley_cat2:" SS_TAP(X_ENTER));
                break;
            case FCAT:
                SEND_STRING(":smiley_cat1:" SS_TAP(X_ENTER));
                break;
            case HART:
                SEND_STRING(":heart:" SS_TAP(X_ENTER));
                break;
            case DROL:
                SEND_STRING(":drooling_face:" SS_TAP(X_ENTER));
                break;
            case MONY:
                SEND_STRING(":smiley_take:" SS_TAP(X_ENTER));
                break;
            case FHAT:
                SEND_STRING(":smiley_fedora:" SS_TAP(X_ENTER));
                break;
            case SPIT:
                SEND_STRING(":smiley_spit:" SS_TAP(X_ENTER));
                break;
            case KING:
                SEND_STRING(":pepe_king:" SS_TAP(X_ENTER));
                break;
            case FLEX:
                SEND_STRING(":muscle:" SS_TAP(X_ENTER));
                break;
            case HAHA:
                SEND_STRING(":smiley_kekw:" SS_TAP(X_ENTER));
                break;
            case LCRY:
                SEND_STRING(":joy:" SS_TAP(X_ENTER));
                break;
            case NICE:
                SEND_STRING(":nice~2:" SS_TAP(X_ENTER));
                break;
            case PPOG:
                SEND_STRING(":pepe_pog:" SS_TAP(X_ENTER));
                break;
            case PRAY:
                SEND_STRING(":pray_tone2:" SS_TAP(X_ENTER));
                break;
            case SWET:
                SEND_STRING(":sweat_smile:" SS_TAP(X_ENTER));
                break;
            case COLD:
                SEND_STRING(":cold_face:" SS_TAP(X_ENTER));
                break;
            case THUM:
                SEND_STRING(":thumbsup:" SS_TAP(X_ENTER));
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
       KC_ESC,   KC_F1,   KC_F2,   KC_F3,   KC_F4,                KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,               KC_F10,  KC_F11,  KC_F12,  KC_F13,  KC_F14,                 COPY,    PAST,    SNIP,    EXPL,    QUIT, 
         QUIT,    TASK,      KC_GRV,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINS,  KC_EQL, KC_BSPC,      KC_INS,  KC_DEL, KC_PGUP,      KC_DEL,  KC_NUM, TG(_FN), KC_PSLS, 
      DM_PLY1, DM_PLY2,      KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P, KC_LBRC, KC_RBRC,              KC_HOME,  KC_END, KC_PGDN,     KC_HOME,   KC_UP, KC_PGUP, KC_PSCR, 
         SNIP,    EXPL,     KC_CAPS,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT,  KC_ENT,                         KC_UP,              KC_LEFT, _______, KC_RGHT, KC_PMNS, 
         COPY,    PAST,     KC_LSFT,    KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH, KC_RSFT,                       KC_LEFT, KC_DOWN, KC_RGHT,      KC_END, KC_DOWN, KC_PGDN, KC_PPLS, 
         VDLT,    VDRT,              MO(_FN),                                      KC_SPC,                                     KC_RALT,                                KC_DOWN,               KC_INS,  KC_DEL, KC_PENT
    ),
    
    
    [_NUM] = LAYOUT(
      _______, _______, _______, _______, _______,              _______, _______, _______, _______, _______,              _______, _______, _______, _______, _______,              _______, _______, _______, _______, _______, 
      _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,     _______, _______, _______,     _______, _______, _______, _______, 
      _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,              _______, _______, _______,        KC_7,    KC_8,    KC_9, KC_PAST, 
      _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                       _______,                 KC_4,    KC_5,    KC_6, _______, 
      _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                       _______, _______, _______,        KC_1,    KC_2,    KC_3, _______, 
      _______, _______,              _______,                                     _______,                                     _______,                                _______,                 KC_0, KC_PDOT, _______
    ),


  [_FN] = LAYOUT(
      QK_BOOT, _______, _______, _______, _______,              _______, _______, _______, _______, _______,              _______, _______, _______, _______, _______,              _______, _______, _______, _______, _______, 
      _______, _______,     QK_BOOT,   KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,  KC_F10,  KC_F11,  KC_F12,  KC_DEL,     _______, _______, _______,     _______, _______, _______, _______, 
      DM_REC1, DM_REC2,        SNIP,    THIS,     OOF,    PIKA,    SCAT,    FCAT,    HART,    DROL,    MONY,    FHAT,    SPIT, _______, _______,              _______, _______, _______,     _______,    VDUP, _______, _______, 
      _______, _______,     KC_LCTL, C(KC_A), C(KC_S), C(KC_D), C(KC_F), _______, _______, _______, _______, _______, KC_VOLD, KC_VOLU, _______,                          VDUP,                 VDLT,    QUIT,    VDRT, _______, 
      _______, _______,     _______, C(KC_Z), C(KC_X), C(KC_C), C(KC_V), _______, _______, _______, _______, KC_MPRV, KC_MNXT, KC_MPLY,                         VDLT,     VDDN,    VDRT,     _______,    VDDN, _______, _______, 
      _______, _______,              MO(_FN),                                     _______,                                     _______,                                   VDDN,              _______, _______, _______
   ),
};

