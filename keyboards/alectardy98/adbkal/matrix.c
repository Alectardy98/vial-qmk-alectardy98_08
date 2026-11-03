// Mixed GPIO matrix: 10x8 scanned keys plus direct GPIO keys and locking Caps Lock.
// CUSTOM_MATRIX=lite: return raw changes; matrix_common.c debounces all 12 rows.
#include "quantum.h"
#include "gpio.h"

static const pin_t row_pins[] = {GP15, GP8, GP0, GP17, GP11, GP12, GP13, GP14, GP16, GP18};
static const pin_t col_pins[] = {GP23, GP22, GP21, GP29, GP28, GP27, GP26, GP25};
// Five original direct keys, EC11 push, and Power, per the uploaded PCB mapping.
static const pin_t direct_pins[] = {GP20, GP7, GP9, GP10, GP6, GP19};

void matrix_init_custom(void) {
    for (uint8_t r = 0; r < 10; r++) setPinInputHigh(row_pins[r]);
    for (uint8_t c = 0; c < 8; c++) setPinInputHigh(col_pins[c]);
    for (uint8_t i = 0; i < 6; i++) setPinInputHigh(direct_pins[i]);
    setPinInputHigh(GP24);
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    matrix_row_t sampled[MATRIX_ROWS] = {0};
    for (uint8_t c = 0; c < 8; c++) {
        setPinOutput(col_pins[c]);
        writePinLow(col_pins[c]);
        wait_us(30);
        for (uint8_t r = 0; r < 10; r++) {
            if (!readPin(row_pins[r])) sampled[r] |= (matrix_row_t)1 << c;
        }
        setPinInputHigh(col_pins[c]);
    }
    for (uint8_t i = 0; i < 6; i++) {
        if (!readPin(direct_pins[i])) sampled[10] |= (matrix_row_t)1 << i;
    }
    if (!readPin(GP24)) sampled[11] |= 1;

    bool raw_changed = false;
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        if (current_matrix[r] != sampled[r]) {
            current_matrix[r] = sampled[r];
            raw_changed = true;
        }
    }
    return raw_changed;
}
