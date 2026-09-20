#include QMK_KEYBOARD_H
#include "print.h"

// List of pins to probe (ATmega32U4). We intentionally skip C6 (RESET).
// Feel free to add/remove pins for your board.
typedef struct {
    pin_t pin;
    const char *name;
    bool was_low;
} probe_pin_t;

// Safe/common GPIO set for 32U4 projects.
// Includes your matrix pins D0/D1 and your 74HC595 pins B1/B2/B3 per README.
static probe_pin_t probes[] = {
    // Port B
    {B0, "B0", false}, {B1, "B1", false}, {B2, "B2", false}, {B3, "B3", false},
    {B4, "B4", false}, {B5, "B5", false}, {B6, "B6", false}, {B7, "B7", false},

    // Port D
    {D0, "D0", false}, {D1, "D1", false}, {D2, "D2", false}, {D3, "D3", false},
    {D4, "D4", false}, {D5, "D5", false}, {D6, "D6", false}, {D7, "D7", false},

    // Port E
    {E6, "E6", false},

    // Port F
    {F0, "F0", false}, {F1, "F1", false}, {F4, "F4", false},
    {F5, "F5", false}, {F6, "F6", false}, {F7, "F7", false},
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // Your original minimal keymap (1x1 matrix)
    [0] = LAYOUT(KC_A)
};

// Configure pins as inputs with pull-ups after USB init
void keyboard_post_init_user(void) {
    for (uint8_t i = 0; i < ARRAY_SIZE(probes); i++) {
        // Set as input with pull-up so an external GND will read LOW
        setPinInputHigh(probes[i].pin);
    }
    uprintf("Pin probe ready. Ground any listed pin to identify it.\r\n");
    uprintf("NOTE: Do NOT ground RESET (C6) or power pins.\r\n");
}

// Scan for transitions to LOW and print once per press
void matrix_scan_user(void) {
    for (uint8_t i = 0; i < ARRAY_SIZE(probes); i++) {
        bool low_now = !readPin(probes[i].pin); // LOW means grounded
        if (low_now && !probes[i].was_low) {
            uprintf("%s grounded\r\n", probes[i].name);
        }
        probes[i].was_low = low_now;
    }
}
