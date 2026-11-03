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

/* Generated from the verified defaults/capture.json; regenerate with
 * tools/generate-defaults.py from this board directory.
 */
enum layer_names {  // Define layers
    _BASE,
    _FN,
    _LAYER2,
    _LAYER3
};

/* Keep this order aligned with main_keycodes.h: Vial stores these values. */
enum custom_keycodes {
    STORAGE = QK_KB_0,      // Enter or leave USB storage mode
    GIF_NEXT,               // Play the next saved GIF
    GIF_PREV,               // Play the previous saved GIF
    GIF_01,                 // Play GIF slot 1
    GIF_02,                 // Play GIF slot 2
    GIF_03,                 // Play GIF slot 3
    GIF_04,                 // Play GIF slot 4
    GIF_05,                 // Play GIF slot 5
    GIF_06,                 // Play GIF slot 6
    GIF_07,                 // Play GIF slot 7
    GIF_08,                 // Play GIF slot 8
    GIF_09,                 // Play GIF slot 9
    GIF_10,                 // Play GIF slot 10
    NES_MODE,               // Enter or leave NES mode
    DOOM_MODE,              // Enter or leave Doom mode
    OLED_DFLT,              // Return to the normal animation display
    GBC_MODE,               // Enter or leave Game Boy Color mode
    OLED_MODE,              // Cycle normal display, NES, GBC and Doom
};

/* _______ = transparent; KC_NO = no key action.
 * Keep the full 8x16 matrix: LAYOUT_capture omits some saved entries.
 * Rows 6 and 7 contain the additional numpad keys.
 */
// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = {
        {    KC_ESC,     KC_F1,     KC_F2,     KC_F3,     KC_F4,     KC_F5,     KC_F6,     KC_F7,     KC_F8,     KC_F9,    KC_F10,    KC_F11,    KC_F12,   KC_PSCR,   KC_PSCR,    KC_INS },
        {    KC_GRV,      KC_1,      KC_2,      KC_3,      KC_4,      KC_5,      KC_6,      KC_7,      KC_8,      KC_9,      KC_0,   KC_MINS,    KC_EQL,   KC_BSPC,   KC_HOME,    KC_DEL },
        {    KC_TAB,      KC_Q,      KC_W,      KC_E,      KC_R,      KC_T,      KC_Y,      KC_U,      KC_I,      KC_O,      KC_P,   KC_LBRC,   KC_RBRC,   KC_BSLS,   KC_PGUP,   KC_PGDN },
        {   KC_CAPS,      KC_A,      KC_S,      KC_D,      KC_F,      KC_G,      KC_H,      KC_J,      KC_K,      KC_L,   KC_SCLN,   KC_QUOT,    KC_ENT,     KC_NO,     KC_NO,     KC_NO },
        {   KC_LSFT,   KC_NUBS,      KC_Z,      KC_X,      KC_C,      KC_V,      KC_B,      KC_N,      KC_M,   KC_COMM,    KC_DOT,   KC_SLSH,   KC_RSFT,   MO(_FN),     KC_UP,   KC_RGHT },
        {   KC_LCTL,     KC_NO,   KC_LALT, OLED_MODE,     KC_NO,     KC_NO,    KC_SPC,     KC_NO,     KC_NO,     KC_NO,     KC_NO,   KC_RALT,     KC_NO,   KC_RCTL,   KC_LEFT,   KC_DOWN },
        {     KC_NO,     KC_NO,     KC_NO,   MO(_FN),    KC_NUM,   KC_PSLS,   KC_PAST,   KC_PMNS,     KC_P7,     KC_P8,     KC_P9,   KC_PPLS,     KC_NO,     KC_NO,     KC_NO,     KC_NO },
        {     KC_P4,     KC_P5,     KC_P6,   KC_PPLS,     KC_P1,     KC_P2,     KC_P3,   KC_PENT,     KC_P0,     KC_P0,   KC_PDOT,   KC_PENT,     KC_NO,     KC_NO,     KC_NO,     KC_NO },
    },

    [_FN] = {
        {   QK_BOOT,    GIF_01,    GIF_02,    GIF_03,    GIF_04,    GIF_05,    GIF_06,    GIF_07,    GIF_08,    GIF_09,    GIF_10,    GIF_10,   _______,   _______,  NES_MODE,  GBC_MODE },
        {   _______,     KC_F1,     KC_F2,     KC_F3,     KC_F4,     KC_F5,     KC_F6,     KC_F7,     KC_F8,     KC_F9,    KC_F10,    KC_F11,    KC_F12,    KC_DEL,   _______, DOOM_MODE },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,     KC_NO,     KC_NO,     KC_NO },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______ },
        {   _______,     KC_NO,   _______,  GIF_NEXT,     KC_NO,     KC_NO,   _______,     KC_NO,     KC_NO,     KC_NO,     KC_NO,   _______,     KC_NO,   _______,   _______,   _______ },
        {     KC_NO,     KC_NO,     KC_NO,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,     KC_NO,     KC_NO,     KC_NO,     KC_NO },
        {   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,   _______,     KC_NO,     KC_NO,     KC_NO,     KC_NO },
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
