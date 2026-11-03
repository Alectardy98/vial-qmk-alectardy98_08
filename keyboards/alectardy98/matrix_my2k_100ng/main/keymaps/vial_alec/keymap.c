/* Copyright 2026 Alectardy98
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
#include "main_keycodes.h"

/* Personal defaults from keymaps/vial_alec/defaults/capture.json.
 * Regenerate the array with tools/generate-defaults.py --keymap vial_alec;
 * the personal macros and their process_record_user implementation are retained.
 */
enum layer_names { // Define layers
    _BASE,
    _FN,
    _LAYER2,
    _LAYER3
};

/* Keep this order aligned with main_keycodes.h: Vial stores these values. */
enum custom_keycodes {
    STORAGE = QK_KB_0, // Enter or leave USB storage mode
    GIF_NEXT,          // Play the next saved GIF
    GIF_PREV,          // Play the previous saved GIF
    GIF_01,            // Play GIF slot 1
    GIF_02,            // Play GIF slot 2
    GIF_03,            // Play GIF slot 3
    GIF_04,            // Play GIF slot 4
    GIF_05,            // Play GIF slot 5
    GIF_06,            // Play GIF slot 6
    GIF_07,            // Play GIF slot 7
    GIF_08,            // Play GIF slot 8
    GIF_09,            // Play GIF slot 9
    GIF_10,            // Play GIF slot 10
    NES_MODE,          // Enter or leave NES mode
    DOOM_MODE,         // Enter or leave Doom mode
    OLED_DFLT,         // Return to the normal animation display
    GBC_MODE,          // Enter or leave Game Boy Color mode
    OLED_MODE,         // Cycle normal display, NES, GBC and Doom
};

/* Personal macros follow the 18 existing MY-2K keycodes. Keep this order
 * aligned with vial.json: customKeycodes indexes are stored in Vial maps.
 */
enum blender_keycode {
    TEST = MY2K_OLED_MODE_TOGGLE + 1, // Type TEST
    VDRT,                             // Virtual Desktop Right
    VDLT,                             // Virtual Desktop Left
    VDUP,                             // Virtual Desktop Up
    VDDN,                             // Virtual Desktop Down
    EXPL,                             // File Explorer
    SNIP,                             // Screenshot Snip Tool
    COPY,                             // Copy All
    CUTT,                             // Cut All
    PAST,                             // Paste Over
    TASK,                             // Task Manager
    GOGL,                             // Open Google Chrome (host shortcut Ctrl+Alt+G)
    CMD,                              // Open Command Prompt (host shortcut Ctrl+Alt+T)
    QUIT,                             // Close App
    DZRO,                             // Double Zero
    ATAB,                             // Alt + Tab
    LPRC,                             // Left Parenthesis
    RPRC,                             // Right Parenthesis
    NUM,                              // Custom Num Lock
    THIS,                             // :this: emoji on Discord
    OOF,                              // :oof: emoji on Discord
    PIKA,                             // :surprised: emoji on Discord
    SCAT,                             // :smiley_cat2: emoji on Discord
    FCAT,                             // :smiley_cat1: emoji on Discord
    HART,                             // :heart: emoji on Discord
    DROL,                             // :drooling_face: emoji on Discord
    MONY,                             // :smiley_take: emoji on Discord
    FHAT,                             // :smiley_fedora: emoji on Discord
    SPIT,                             // :smiley_spit: emoji on Discord
    KING,                             // :pepe_king: emoji on Discord
    FLEX,                             // :muscle: emoji on Discord
    HAHA,                             // :smiley_kekw: emoji on Discord
    LCRY,                             // :joy: emoji on Discord
    NICE,                             // :nice~2: emoji on Discord
    PPOG,                             // :pepe_pog: emoji on Discord
    PRAY,                             // :pray_tone2: emoji on Discord
    SWET,                             // :sweat_smile: emoji on Discord
    COLD,                             // :cold_face: emoji on Discord
    THUM,                             // :thumbsup: emoji on Discord
};

_Static_assert((int)TEST == (int)QK_KB_18, "Update the Vial custom-keycode prefix before moving macros");
_Static_assert(THUM - QK_KB_0 < 64, "VialAstro supports at most 64 custom keycodes");

/* _______ = transparent; KC_NO = no key action.
 * Keep the full 8x16 matrix: LAYOUT_capture omits some saved entries.
 * Rows 6 and 7 contain the additional numpad keys.
 */
// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = {
        {    KC_ESC,     KC_F1,     KC_F2,     KC_F3,     KC_F4,     KC_F5,     KC_F6,     KC_F7,     KC_F8,     KC_F9,    KC_F10,    KC_F11,    KC_F12,   KC_PSCR,   KC_PSCR,    KC_INS },
        {    KC_GRV,      KC_1,      KC_2,      KC_3,      KC_4,      KC_5,      KC_6,      KC_7,      KC_8,      KC_9,      KC_0,   KC_MINS,    KC_EQL,   KC_BSPC,      COPY,    KC_DEL },
        {    KC_TAB,      KC_Q,      KC_W,      KC_E,      KC_R,      KC_T,      KC_Y,      KC_U,      KC_I,      KC_O,      KC_P,   KC_LBRC,   KC_RBRC,   KC_BSLS,      PAST,      EXPL },
        {   KC_CAPS,      KC_A,      KC_S,      KC_D,      KC_F,      KC_G,      KC_H,      KC_J,      KC_K,      KC_L,   KC_SCLN,   KC_QUOT,    KC_ENT,     KC_NO,     KC_NO,     KC_NO },
        {   KC_LSFT,   KC_NUBS,      KC_Z,      KC_X,      KC_C,      KC_V,      KC_B,      KC_N,      KC_M,   KC_COMM,    KC_DOT,   KC_SLSH,   KC_RSFT,   MO(_FN),     KC_UP,   KC_RGHT },
        {   MO(_FN),     KC_NO,   KC_LALT, OLED_MODE,     KC_NO,     KC_NO,    KC_SPC,     KC_NO,     KC_NO,     KC_NO,     KC_NO,   KC_RALT,     KC_NO,   KC_RCTL,   KC_LEFT,   KC_DOWN },
        {     KC_NO,     KC_NO,     KC_NO,      TASK,    KC_NUM,   KC_PSLS,   KC_PAST,   KC_PMNS,     KC_P7,     KC_P8,     KC_P9,   KC_PPLS,     KC_NO,     KC_NO,     KC_NO,     KC_NO },
        {     KC_P4,     KC_P5,     KC_P6,   KC_PPLS,     KC_P1,     KC_P2,     KC_P3,   KC_PENT,     KC_P0,     KC_P0,   KC_PDOT,   KC_PENT,     KC_NO,     KC_NO,     KC_NO,     KC_NO },
    },

    [_FN] = {
        {   QK_BOOT,    GIF_01,    GIF_02,    GIF_03,    GIF_04,    GIF_05,    GIF_06,    GIF_07,    GIF_08,    GIF_09,    GIF_10,  GIF_PREV,  GIF_NEXT,   _______,  NES_MODE,  GBC_MODE },
        {   _______,     KC_F1,     KC_F2,     KC_F3,     KC_F4,     KC_F5,     KC_F6,     KC_F7,     KC_F8,     KC_F9,    KC_F10,    KC_F11,    KC_F12,    KC_DEL,   _______, DOOM_MODE },
        {      SNIP,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   KC_LCTL,    0x0104,    0x0116,    0x0107,    0x0109,   _______,   _______,   _______,   _______,   _______,   KC_VOLD,   KC_VOLU,   _______,     KC_NO,     KC_NO,     KC_NO },
        {   _______,   _______,    0x011D,    0x011B,    0x0106,    0x0119,   _______,   _______,   _______,   KC_MPRV,   KC_MNXT,   KC_MPLY,   _______,   _______,      VDUP,      VDRT },
        {   _______,     KC_NO,   KC_LGUI,  GIF_NEXT,     KC_NO,     KC_NO,   _______,     KC_NO,     KC_NO,     KC_NO,     KC_NO,   _______,     KC_NO,   _______,      VDLT,      VDDN },
        {     KC_NO,     KC_NO,     KC_NO,   STORAGE,   _______,   _______,   _______,   _______,   _______,      VDUP,   _______,   _______,     KC_NO,     KC_NO,     KC_NO,     KC_NO },
        {      VDLT,   _______,      VDRT,   _______,   _______,      VDDN,   _______,   _______,   _______,   _______,   _______,   _______,     KC_NO,     KC_NO,     KC_NO,     KC_NO },
    },

    [_LAYER2] = {
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
    },

    [_LAYER3] = {
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
    },
};
// clang-format on

/* Only emit a macro on press; consume its release as well. Board controls,
 * ordinary keys, game-mode routing and storage-mode gating remain in Main. */
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
#ifdef CONSOLE_ENABLE
    uprintf("KL: kc: 0x%04X, col: %u, row: %u, pressed: %1u, time: %u, interrupt: %1u, count: %u\n", keycode,
            record->event.key.col, record->event.key.row, record->event.pressed, record->event.time,
            record->tap.interrupted, record->tap.count);
#endif
    if (keycode < TEST || keycode > THUM)
        return true;
    if (!record->event.pressed)
        return false;

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
            SEND_STRING(SS_LCTL(SS_DOWN(X_RSFT) SS_TAP(X_ESCAPE) SS_UP(X_RSFT))); // Windows
            SEND_STRING(SS_LCTL(SS_DOWN(X_LALT) SS_TAP(X_ESCAPE) SS_UP(X_LALT))); // Mac
            break;
        case GOGL:
            SEND_STRING(SS_LCTL(SS_LALT(SS_TAP(X_G))));
            break;
        case CMD:
            SEND_STRING(SS_LCTL(SS_LALT(SS_TAP(X_T))));
            break;
        case QUIT:
            SEND_STRING(SS_LALT(SS_TAP(X_F4))); // Windows
            SEND_STRING(SS_LCTL(SS_TAP(X_W)));  // Mac
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
            SEND_STRING(SS_TAP(X_NUM));
            break;

        // Discord messages include Enter, as requested.
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
    return false;
}
