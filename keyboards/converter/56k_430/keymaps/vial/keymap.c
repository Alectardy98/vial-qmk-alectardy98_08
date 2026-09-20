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
       CLCK,    BEPR,       KC_F1,   KC_F2,   KC_F3,                     KC_F4,   KC_F5,                              KC_F6,   KC_F7,   KC_F8,                        KC_F9,  KC_F10,  KC_F11,
      KC_F1,   KC_F2,                                                                                                                                                KC_F12, KC_PAUS, KC_PSCR,
      KC_F3,   KC_F4,      KC_ESC,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINS,  KC_EQL, KC_BSPC,  KC_DEL,     KC_INS, KC_HOME, KC_PGUP,
      KC_F5,   KC_F6,      KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P, KC_LBRC, KC_RBRC, KC_BSLS,  KC_GRV,     KC_DEL,  KC_END, KC_PGDN,
      KC_F7,   KC_F8,     KC_CAPS,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT,  KC_ENT,  KC_ENT,             KC_MPRV, KC_MPLY, KC_PGDN,
      KC_F9,  KC_F10,     KC_LSFT,             KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH,          KC_RSFT,             KC_VOLD,   KC_UP, KC_VOLU,
     KC_F11,  KC_F12,                       MO(_FN),                             KC_SPC,                   KC_RCTL,                   KC_RALT,                      KC_LEFT, KC_DOWN, KC_RGHT
        ),
    [_FN] = LAYOUT(
    _______, _______,     _______, _______, _______,                   _______, _______,                            _______, _______, _______,                      _______, _______, _______,
    _______, _______,                                                                                                                                               _______, _______, _______,
    _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,    _______, _______, _______,
    _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,    _______, _______, _______,
    _______, _______,     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,             _______, _______, _______,
    _______, _______,     _______,          _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______,             _______, _______, _______,
    _______, _______,                       _______,                            _______,                   _______,                   _______,                      _______, _______, _______
        ),

};
