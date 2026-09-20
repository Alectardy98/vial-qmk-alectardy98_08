#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <util/atomic.h>

#include "wait.h"
#include "matrix.h"
#include "debug.h"
#include "gpio.h"

// Keep your indicator
#define INDICATOR B7

extern matrix_row_t matrix[MATRIX_ROWS];

// --- Tunables (robust defaults) ---
#ifndef MATRIX_SETTLE_US
#    define MATRIX_SETTLE_US 30
#endif

// Number of times to sample PIND after settle; AND together to reject brief highs.
// If your hardware is active-low, this should be OR instead of AND (see note below).
#ifndef MATRIX_SAMPLES
#    define MATRIX_SAMPLES 3
#endif

// How many consecutive identical raw scans are required before we "commit" to QMK matrix.
// 1 = no debounce, 2-4 typical.
#ifndef MATRIX_STABLE_COUNT
#    define MATRIX_STABLE_COUNT 2
#endif

static inline uint8_t popcount16(uint16_t x) {
    uint8_t c = 0;
    while (x) { c += (x & 1); x >>= 1; }
    return c;
}

// Standard ghosting heuristic: if 2+ rows have 2+ keys and share any column -> ghosting possible
static bool has_ghosting(const matrix_row_t *m) {
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
        uint16_t a = m[i];
        if (popcount16(a) < 2) continue;

        for (uint8_t j = i + 1; j < MATRIX_ROWS; j++) {
            uint16_t b = m[j];
            if (popcount16(b) < 2) continue;

            if (a & b) return true;
        }
    }
    return false;
}

void matrix_init_user(void) {
    debug_enable = false;
    debug_matrix = false;

    // indicator LED
    setPinOutput(INDICATOR);
    writePinHigh(INDICATOR);

    // row address outputs (4-bit address)
    setPinOutput(F0); writePinLow(F0);
    setPinOutput(F1); writePinLow(F1);
    setPinOutput(F4); writePinLow(F4);
    setPinOutput(F5); writePinLow(F5);

    // column inputs with pull-ups (per-pin, don’t clobber whole PORTD)
    setPinInputHigh(D0);
    setPinInputHigh(D1);
    setPinInputHigh(D2);
    setPinInputHigh(D3);
    setPinInputHigh(D4);
    setPinInputHigh(D5);
    setPinInputHigh(D6);
    setPinInputHigh(D7);
}

void matrix_clear(void) {
    for (uint8_t x = 0; x < MATRIX_ROWS; x++) {
        matrix[x] = 0;
    }
}

// NOTE: This keeps your "reversed" logic:
// - We iterate r over MATRIX_COLS (your addressed rows, 0..15)
// - We store into new_matrix[c] where c is 0..7 (your read columns D0..D7)
// - We set bit r in that row_t (so each "row" contains 16 bits)
bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    static matrix_row_t last_raw[MATRIX_ROWS] = {0};
    static uint8_t      stable[MATRIX_ROWS]   = {0};

    matrix_row_t raw[MATRIX_ROWS] = {0};

    // Scan addressed rows (0..15)
    for (uint8_t r = 0; r < MATRIX_COLS; r++) {
        // drive row address
        writePin(F0, (r >> 0) & 1);
        writePin(F1, (r >> 1) & 1);
        writePin(F4, (r >> 2) & 1);
        writePin(F5, (r >> 3) & 1);

        wait_us(MATRIX_SETTLE_US);

        // Multi-sample PIND to reject brief glitches.
        // This assumes "pressed reads HIGH". If your matrix is active-low, see note below.
        uint8_t pins = 0xFF;
        for (uint8_t s = 0; s < MATRIX_SAMPLES; s++) {
            uint8_t p = PIND;
            pins &= p;           // active-high stable-high filter
            wait_us(2);
        }

        // transpose: each column bit goes into raw[col] with row-bit set
        for (uint8_t c = 0; c < MATRIX_ROWS; c++) {
            if (pins & (1 << c)) {
                raw[c] |= ((matrix_row_t)1 << r);
            }
        }
    }

    // Ghosting filter:
    // If ghosting is detected, DO NOT introduce any *new* presses (phantoms).
    // Still allow releases, so keys don't get stuck.
    if (has_ghosting(raw)) {
        for (uint8_t c = 0; c < MATRIX_ROWS; c++) {
            raw[c] &= current_matrix[c];
        }
    }

    bool changed = false;

    // Debounce via stable-scan requirement
    for (uint8_t c = 0; c < MATRIX_ROWS; c++) {
        if (raw[c] == last_raw[c]) {
            if (stable[c] < MATRIX_STABLE_COUNT) stable[c]++;
        } else {
            last_raw[c] = raw[c];
            stable[c]   = 0;
        }

        if (stable[c] >= MATRIX_STABLE_COUNT && current_matrix[c] != raw[c]) {
            current_matrix[c] = raw[c];
            changed = true;
        }
    }

    return changed;
}

/*
IMPORTANT NOTE (only if needed):
If your electrical matrix is "pressed = LOW" (typical with pull-ups + switch to GND),
then change two things above:
  1) pins init for sampling should be 0x00 not 0xFF
  2) sampling should be: pins |= p; (OR) and test should be: if (!(pins & (1<<c)))
But since your current code mostly works, I left it in the same "pressed = HIGH" interpretation.
*/
