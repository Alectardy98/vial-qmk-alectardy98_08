
#include QMK_KEYBOARD_H
#include <avr/io.h>
#include "quantum.h"


enum layer_names {
    _BASE,
    _FN
};

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
   /*
    * ,-----------. ,-----------. ,---------------.
    * | 1C| 1D| 24| | 2D| D5| 3C| | 43| 44| 4C| 71|
    * |-----------| |-----------| |---------------|
    * | 1A| 22| 21| | 2A| 5A| 3A| | 41| 4B| 49| 4A|
    * |-----------| |-----------| |---------------|
    * | 16| 1E| 26| | 25| 09| 3D| | 3E| 46| 45| 52|
    * |-----------| |-----------| |---------------|
    * | 12| 05| 06| | 2E| 01| 36| | 55| 0A| 54| 4E|
    * |-----------| |-----------| |---------------|
    * | 14| 11| 04| | 2C| 66| 35| | 5B| 0B| 83| 33|
    * `-----------' `-----------' `---------------'
    */
         
    [_BASE] = LAYOUT(
                 QK_BOOT,     CMD,    TASK,    KC_INS, KC_HOME, KC_PGUP,      THIS,    NICE,     OOF,    HAHA, \
                 _______, _______, _______,    KC_DEL,  KC_END, KC_PGDN,      SCAT,    FCAT,    FHAT,    MONY, \
                 _______,    SNIP,    EXPL,   KC_MPRV, KC_MPLY, KC_MNXT,      KING,    PPOG,    SPIT,    PIKA, \
                 _______,    COPY,    PAST,   KC_VOLD,   KC_UP, KC_VOLU,      SWET,    LCRY,    DROL,    COLD, \
                 _______,    VDLT,    VDRT,   KC_LEFT, KC_DOWN, KC_RGHT,      FLEX,    THUM,    PRAY,    HART  \
    ),
    [_FN] = LAYOUT(
                 _______, _______, _______,   _______, _______, _______,   _______, _______, _______, _______, \
                 _______, _______, _______,   _______, _______, _______,   _______, _______, _______, _______, \
                 _______, _______, _______,   _______, _______, _______,   _______, _______, _______, _______, \
                 _______, _______, _______,   _______, _______, _______,   _______, _______, _______, _______, \
                 _______, _______, _______,   _______, _______, _______,   _______, _______, _______, _______  \
    ),

};
