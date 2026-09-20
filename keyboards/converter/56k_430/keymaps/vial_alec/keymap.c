#include QMK_KEYBOARD_H
#include "print.h"  // for xprintf()

// Forward-declared from matrix.c
void serial_write(uint8_t b);
extern void walt_send_led_mask(uint8_t mask);

// Layers
enum layers {
    _BASE,
    _FN
};

// Sound modes
typedef enum {
    SOUND_NONE  = 0x00,
    SOUND_CLICK = 0x20,
    SOUND_BEEP  = 0x40,
} sound_mode_t;

static sound_mode_t current_sound_mode = SOUND_NONE;
enum blender_keycode {
    TEST = QK_KB_0,
    CLCK,                   //Desktop Right "set to move right a space on mac"
    BEPR,                   //Desktop Left "set to move left a space on mac"
    OFFX,                   //V-Desktop Up "set to mission controll on mac"
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


// LED update called by QMK automatically
bool led_update_kb(led_t led_state) {
    uint8_t mask = 0;
    if (led_state.caps_lock) mask |= 0x01;
    if (layer_state_is(_FN)) mask |= 0x02;

    xprintf("WALT LED mask = 0x%02X\n", mask);
    walt_send_led_mask(mask);  // only low nibble
    return false;
}

// Trigger LED update when layer changes
layer_state_t layer_state_set_user(layer_state_t state) {
    led_update_kb(host_keyboard_led_state());
    return state;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        switch (keycode) {
            case TEST:
                SEND_STRING("TEST");
                break;
            case CLCK:
                current_sound_mode = (current_sound_mode == SOUND_CLICK) ? SOUND_NONE : SOUND_CLICK;
                return false;
            case BEPR:
                current_sound_mode = (current_sound_mode == SOUND_BEEP) ? SOUND_NONE : SOUND_BEEP;
                return false;
            case OFFX:
                current_sound_mode = SOUND_NONE;
                return false;
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
        

        uint8_t mask = 0;
        if (host_keyboard_led_state().caps_lock) mask |= 0x01;
        if (layer_state_is(_FN)) mask |= 0x02;

        uint8_t tx_byte = current_sound_mode | mask;
        xprintf("TX: 0x%02X\n", tx_byte);
        serial_write(tx_byte);
    }

    return true;
}



const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

	[_BASE] = LAYOUT(
     KC_ESC,    TASK,       KC_F1,   KC_F2,   KC_F3,                     KC_F4,   KC_F5,                              KC_F6,   KC_F7,   KC_F8,                        KC_F9,  KC_F10,  KC_F11,
       CLCK,    BEPR,                                                                                                                                                KC_F12, KC_PAUS, KC_PSCR,
    DM_PLY1, DM_PLY2,      KC_ESC,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINS,  KC_EQL, KC_BSPC,  KC_DEL,     KC_INS, KC_HOME, KC_PGUP,
    _______, _______,      KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P, KC_LBRC, KC_RBRC, KC_BSLS,  KC_GRV,     KC_DEL,  KC_END, KC_PGDN,
       SNIP,    EXPL,     KC_CAPS,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT,  KC_ENT,  KC_ENT,             KC_MPRV, KC_MPLY, KC_PGDN,
       COPY,    PAST,     KC_LSFT,             KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH,          KC_RSFT,             KC_VOLD,   KC_UP, KC_VOLU,
       VDLT,    VDRT,                       MO(_FN),                             KC_SPC,                   KC_RCTL,                   KC_RALT,                      KC_LEFT, KC_DOWN, KC_RGHT
        ),
    [_FN] = LAYOUT(
    QK_BOOT, _______,     _______, _______, _______,                   _______, _______,                            _______, _______, _______,                      _______, _______, _______,
    _______, _______,                                                                                                                                               _______, _______, _______,
    DM_REC1, DM_REC2,     QK_BOOT,   KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,  KC_F10,  KC_F11,  KC_F12,  KC_DEL, _______,    _______, _______, _______,
    _______, _______,        SNIP,    THIS,     OOF,    PIKA,    SCAT,    FCAT,    HART,    DROL,    MONY,    FHAT,    SPIT, _______, _______, _______, _______,    _______, _______, _______,
    _______, _______,     KC_LCTL, C(KC_A), C(KC_S), C(KC_D), C(KC_F), _______, _______, _______, _______, _______, KC_VOLD, KC_VOLU, _______, _______,             _______, _______, _______,
    _______, _______,     _______,          C(KC_Z), C(KC_X), C(KC_C), C(KC_V), _______, _______, _______, KC_MPRV, KC_MNXT, KC_MPLY,          _______,             _______,    VDUP, _______,
    _______, _______,                       _______,                            _______,                   KC_LGUI,                   _______,                         VDLT,    VDDN,    VDRT
        ),

};
