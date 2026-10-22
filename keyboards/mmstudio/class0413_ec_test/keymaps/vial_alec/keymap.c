/* Copyright 2022 Alectardy98
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include QMK_KEYBOARD_H

enum _layer {
    _BASE,
    _NUM,
    _FN,
    _LAYER3,
};


enum custom_keycodes {
    EC_NOIS = QK_KB_0,
    EC_BOTM,
    EC_PRNT,
    LK_SONG,
    TEST,
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

_Static_assert(EC_NOIS == (int)CLASS0413_EC_NOISE, "EC Noise keycode ABI");
_Static_assert(EC_BOTM == (int)CLASS0413_EC_BOTTOM_START, "EC Bottom keycode ABI");
_Static_assert(EC_PRNT == (int)CLASS0413_EC_CAL_PRINT, "EC Print keycode ABI");

_Static_assert(LK_SONG == (int)CLASS0413_LK_SONG, "Lock song keycode ABI");


// Same encoded send-string commands; share dispatch instead of 36 call sites.
static const char *const macro_commands[THUM - TEST + 1] = {
    [TEST - TEST] = "TEST",
    [VDRT - TEST] = SS_LGUI(SS_DOWN(X_LCTL) SS_TAP(X_RIGHT) SS_UP(X_LCTL)),
    [VDLT - TEST] = SS_LGUI(SS_DOWN(X_LCTL) SS_TAP(X_LEFT) SS_UP(X_LCTL)),
    [VDUP - TEST] = SS_LGUI(SS_TAP(X_TAB) SS_TAP(X_T)),
    [VDDN - TEST] = SS_LGUI("d"),
    [EXPL - TEST] = SS_LGUI("e"),
    [SNIP - TEST] = SS_LGUI(SS_LSFT("s")),
    [COPY - TEST] = SS_TAP(X_F2) SS_LCTL(SS_TAP(X_A) SS_TAP(X_C)),
    [CUTT - TEST] = SS_TAP(X_F2) SS_LCTL(SS_TAP(X_A) SS_TAP(X_X)),
    [PAST - TEST] = SS_TAP(X_F2) SS_LCTL(SS_TAP(X_V)),
    [GOGL - TEST] = SS_LCTL(SS_LALT(SS_TAP(X_G))),
    [CMD - TEST] = SS_LCTL(SS_LALT(SS_TAP(X_T))),
    [DZRO - TEST] = "00",
    [ATAB - TEST] = SS_LALT(SS_TAP(X_TAB)),
    [LPRC - TEST] = "(",
    [RPRC - TEST] = ")",
    [THIS - TEST] = ":this:" SS_TAP(X_ENTER),
    [OOF - TEST] = ":oof:" SS_TAP(X_ENTER),
    [PIKA - TEST] = ":surprised:" SS_TAP(X_ENTER),
    [SCAT - TEST] = ":smiley_cat2:" SS_TAP(X_ENTER),
    [FCAT - TEST] = ":smiley_cat1:" SS_TAP(X_ENTER),
    [HART - TEST] = ":heart:" SS_TAP(X_ENTER),
    [DROL - TEST] = ":drooling_face:" SS_TAP(X_ENTER),
    [MONY - TEST] = ":smiley_take:" SS_TAP(X_ENTER),
    [FHAT - TEST] = ":smiley_fedora:" SS_TAP(X_ENTER),
    [SPIT - TEST] = ":smiley_spit:" SS_TAP(X_ENTER),
    [KING - TEST] = ":pepe_king:" SS_TAP(X_ENTER),
    [FLEX - TEST] = ":muscle:" SS_TAP(X_ENTER),
    [HAHA - TEST] = ":smiley_kekw:" SS_TAP(X_ENTER),
    [LCRY - TEST] = ":joy:" SS_TAP(X_ENTER),
    [NICE - TEST] = ":nice~2:" SS_TAP(X_ENTER),
    [PPOG - TEST] = ":pepe_pog:" SS_TAP(X_ENTER),
    [PRAY - TEST] = ":pray_tone2:" SS_TAP(X_ENTER),
    [SWET - TEST] = ":sweat_smile:" SS_TAP(X_ENTER),
    [COLD - TEST] = ":cold_face:" SS_TAP(X_ENTER),
    [THUM - TEST] = ":thumbsup:" SS_TAP(X_ENTER),
};

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed && keycode >= TEST && keycode <= THUM) {
        const char *command = macro_commands[keycode - TEST];
        if (command) send_string_with_delay(command, 0);
        else switch (keycode) {
            case TASK:
                SEND_STRING(SS_LCTL(SS_DOWN(X_RSFT) SS_TAP(X_ESCAPE) SS_UP(X_RSFT)));  // Windows
                SEND_STRING(SS_LCTL(SS_DOWN(X_LALT) SS_TAP(X_ESCAPE) SS_UP(X_LALT)));    // Mac
                break;
            case QUIT:
                SEND_STRING(SS_LALT(SS_TAP(X_F4)));     // Windows
                SEND_STRING(SS_LCTL(SS_TAP(X_W)));      // Mac
                break;
            case NUM:
                SEND_STRING(SS_TAP(X_NUM));         // Toggles layer change and presses the num lock button, to allow for Num lock toggle to occur for both Mac OS and Windows
                return true;
                break;
            // Discord Section
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
         KC_ESC,    TASK,      KC_GRV,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINS,  KC_EQL, _______, KC_BSPC,         TG(_NUM), KC_PSLS, KC_PAST, KC_PMNS,
        DM_PLY1, DM_PLY2,      KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P, KC_LBRC, KC_RBRC, KC_BSLS,                      KC_7,    KC_8,    KC_9, KC_PPLS,
           SNIP,    EXPL,     KC_CAPS,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT, _______,  KC_ENT,                      KC_4,    KC_5,    KC_6,
           COPY,    PAST,     KC_LSFT, _______,    KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH, KC_RSFT,  KC_DEL,                      KC_1,    KC_2,    KC_3,
           VDLT,    VDRT,     KC_LCTL, MO(_FN), KC_LALT,                             KC_SPC,                            MO(_FN), KC_LGUI, KC_RCTL,                               KC_0, KC_PDOT, KC_PENT
    ),
    [_NUM] = LAYOUT(
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______,
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                   KC_PGUP,   KC_UP, KC_HOME, _______,
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                   KC_LEFT, _______, KC_RGHT,
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                   KC_PGDN, KC_DOWN,  KC_END,
        _______, _______,     _______, _______, _______,                            _______,                            _______, _______, _______,                             KC_INS,  KC_DEL, _______
    ),
    [_FN] = LAYOUT(
        QK_BOOT, AU_TOGG,      KC_ESC,   KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,  KC_F10,  KC_F11,  KC_F12, _______,  KC_DEL,          EC_NOIS, EC_BOTM, EC_PRNT, QK_CLEAR_EEPROM,
        DM_REC1, DM_REC2,        SNIP,    THIS,     OOF,    PIKA,    FCAT,    HART,    DROL,    MONY,    FHAT,    SPIT,    KING, _______, _______, _______,                   _______,    VDUP, _______, _______,
        LK_SONG, MU_NEXT,     KC_LCTL, C(KC_A), C(KC_S), C(KC_D), C(KC_F), _______, _______, _______, _______, _______, KC_VOLD, KC_VOLU, _______, _______,                      VDLT, _______,    VDRT,
        CK_DOWN,   CK_UP,     _______, _______, C(KC_Z), C(KC_X), C(KC_C), C(KC_V), _______, _______, _______, KC_MPRV, KC_MNXT, KC_MPLY, _______, _______,                   _______,    VDDN, _______,
        CK_TOGG, MU_TOGG,     KC_LGUI, _______, _______,                            HF_TOGG,                            _______, _______, _______,                            _______, _______, _______
    ),
    [_LAYER3] = LAYOUT(
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______,
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                   _______, _______, _______, _______,
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                   _______, _______, _______,
        _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                   _______, _______, _______,
        _______, _______,     _______, _______, _______,                            _______,                            _______, _______, _______,                            _______, _______, _______
    ),
};

