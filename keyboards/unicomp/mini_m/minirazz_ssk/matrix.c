/* matrix.c — custom 12×16 → 6×18 remapping driver
 *
 * Copyright 2023 Purdea Andrei
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "quantum.h"
#include <hardware/structs/pads_qspi.h>

#define SHIFTREG_CLK           GP26
#define SHIFTREG_SHLD          GP27
#define SHIFTREG_DATA          GP24
#define SHIFTREG_DELAY_CYCLES  8   /* ~64 ns @125 MHz */



#define PHYS_ROWS   12
#define PHYS_COLS   16
#define LOGIC_ROWS   6
#define LOGIC_COLS  18

//==============================================================================
// 1) PHYS→LOGIC MAP STUB
//==============================================================================
// phys2log[phys_row][phys_col] = { logic_row, logic_col }
// Fill each {0xFF,0xFF} below with your desired {lr,lc} in [0..5]×[0..17].
// 1) PHYS→LOGIC MAP: fill every occupied [r][c] → next logical (lr,lc).
#define UNMAPPED 0xFF, 0xFF

static const uint8_t phys2log[PHYS_ROWS][PHYS_COLS][2] = {
    // phys row 0
    {
        /* [0][ 0] */ { 1, 0 },
        /* [0][ 1] */ { 0, 0 },
        /* [0][ 2] */ { 1, 5 },
        /* [0][ 3] */ { 4,12 },
        /* [0][ 4] */ { 0, 2 },
        /* [0][ 5] */ { 0, 3 },
        /* [0][ 6] */ { 0, 4 },
        /* [0][ 7] */ { 2,10 },
        /* [0][ 8] */ { UNMAPPED },
        /* [0][ 9] */ { UNMAPPED },
        /* [0][10] */ { UNMAPPED },
        /* [0][11] */ { UNMAPPED },
        /* [0][12] */ { 5, 2 },
        /* [0][13] */ { 3,13 },
        /* [0][14] */ { UNMAPPED },
        /* [0][15] */ { UNMAPPED },
    },
    // phys row 1
    {
        /* [1][ 0] */ { 3, 0 },
        /* [1][ 1] */ { 4, 1 },
        /* [1][ 2] */ { 2, 1 },
        /* [1][ 3] */ { UNMAPPED },
        /* [1][ 4] */ { UNMAPPED },
        /* [1][ 5] */ { UNMAPPED },
        /* [1][ 6] */ { UNMAPPED },
        /* [1][ 7] */ { 1, 6 },
        /* [1][ 8] */ { 0, 5 },
        /* [1][ 9] */ { UNMAPPED },
        /* [1][10] */ { UNMAPPED },
        /* [1][11] */ { UNMAPPED },
        /* [1][12] */ { UNMAPPED },
        /* [1][13] */ { 3,12 },
        /* [1][14] */ { 5, 7 },
        /* [1][15] */ { 4,13 },
    },
    // phys row 2
    {
        /* [2][ 0] */ { 3, 2 },
        /* [2][ 1] */ { 3, 3 },
        /* [2][ 2] */ { 0, 1 },
        /* [2][ 3] */ { 3, 4 },
        /* [2][ 4] */ { 3, 5 },
        /* [2][ 5] */ { 4, 2 },
        /* [2][ 6] */ { 4, 4 },
        /* [2][ 7] */ { UNMAPPED },
        /* [2][ 8] */ { 4, 3 },
        /* [2][ 9] */ { 4, 5 },
        /* [2][10] */ { 4, 6 },
        /* [2][11] */ { 3, 1 },
        /* [2][12] */ { 2, 0 },
        /* [2][13] */ { UNMAPPED },
        /* [2][14] */ { UNMAPPED },
        /* [2][15] */ { UNMAPPED },
    },
    // phys row 3
    {
        /* [3][ 0] */ { UNMAPPED },
        /* [3][ 1] */ { UNMAPPED },
        /* [3][ 2] */ { 1, 1 },
        /* [3][ 3] */ { UNMAPPED },
        /* [3][ 4] */ { UNMAPPED },
        /* [3][ 5] */ { UNMAPPED },
        /* [3][ 6] */ { UNMAPPED },
        /* [3][ 7] */ { 2, 6 },
        /* [3][ 8] */ { 0, 6 },
        /* [3][ 9] */ { UNMAPPED },
        /* [3][10] */ { UNMAPPED },
        /* [3][11] */ { 5, 1 },
        /* [3][12] */ { UNMAPPED },
        /* [3][13] */ { UNMAPPED },
        /* [3][14] */ { UNMAPPED },
        /* [3][15] */ { UNMAPPED },
    },
    // phys row 4
    {
        /* [4][ 0] */ { UNMAPPED },
        /* [4][ 1] */ { UNMAPPED },
        /* [4][ 2] */ { 1, 4 },
        /* [4][ 3] */ { UNMAPPED },
        /* [4][ 4] */ { UNMAPPED },
        /* [4][ 5] */ { UNMAPPED },
        /* [4][ 6] */ { UNMAPPED },
        /* [4][ 7] */ { 2, 9 },
        /* [4][ 8] */ { 0, 7 },
        /* [4][ 9] */ { UNMAPPED },
        /* [4][10] */ { UNMAPPED },
        /* [4][11] */ { UNMAPPED },
        /* [4][12] */ { UNMAPPED },
        /* [4][13] */ { UNMAPPED },
        /* [4][14] */ { 5, 0 },
        /* [4][15] */ { 4, 0 },
    },
    // phys row 5
    {
        /* [5][ 0] */ { UNMAPPED },
        /* [5][ 1] */ { UNMAPPED },
        /* [5][ 2] */ { 2, 2 },
        /* [5][ 3] */ { UNMAPPED },
        /* [5][ 4] */ { UNMAPPED },
        /* [5][ 5] */ { UNMAPPED },
        /* [5][ 6] */ { UNMAPPED },
        /* [5][ 7] */ { 1, 7 },
        /* [5][ 8] */ { 0, 8 },
        /* [5][ 9] */ { 2, 11 },
        /* [5][10] */ { 5, 5 },
        /* [5][11] */ { UNMAPPED },
        /* [5][12] */ { UNMAPPED },
        /* [5][13] */ { UNMAPPED },
        /* [5][14] */ { UNMAPPED },
        /* [5][15] */ { UNMAPPED },
    },
    // phys row 6
    {
        /* [6][ 0] */ { UNMAPPED },
        /* [6][ 1] */ { UNMAPPED },
        /* [6][ 2] */ { 1, 2 },
        /* [6][ 3] */ { UNMAPPED },
        /* [6][ 4] */ { UNMAPPED },
        /* [6][ 5] */ { UNMAPPED },
        /* [6][ 6] */ { UNMAPPED },
        /* [6][ 7] */ { 2, 7 },
        /* [6][ 8] */ { 0, 9 },
        /* [6][ 9] */ { 1,12 },
        /* [6][10] */ { 0,10 },
        /* [6][11] */ { 0,11 },
        /* [6][12] */ { 1,11 },
        /* [6][13] */ { 0,13 },
        /* [6][14] */ { UNMAPPED },
        /* [6][15] */ { UNMAPPED },
    },
    // phys row 7
    {
        /* [7][ 0] */ { UNMAPPED },
        /* [7][ 1] */ { UNMAPPED },
        /* [7][ 2] */ { 1, 3 },
        /* [7][ 3] */ { UNMAPPED },
        /* [7][ 4] */ { UNMAPPED },
        /* [7][ 5] */ { UNMAPPED },
        /* [7][ 6] */ { UNMAPPED },
        /* [7][ 7] */ { 2, 8 },
        /* [7][ 8] */ { UNMAPPED },
        /* [7][ 9] */ { UNMAPPED },
        /* [7][10] */ { UNMAPPED },
        /* [7][11] */ { 0,12 },
        /* [7][12] */ { 1,13 },
        /* [7][13] */ { 0,14 },
        /* [7][14] */ { UNMAPPED },
        /* [7][15] */ { UNMAPPED },
    },
    // phys row 8
    {
        /* [8][ 0] */ { UNMAPPED },
        /* [8][ 1] */ { UNMAPPED },
        /* [8][ 2] */ { 2, 5 },
        /* [8][ 3] */ { UNMAPPED },
        /* [8][ 4] */ { UNMAPPED },
        /* [8][ 5] */ { UNMAPPED },
        /* [8][ 6] */ { UNMAPPED },
        /* [8][ 7] */ { 1,10 },
        /* [8][ 8] */ { UNMAPPED },
        /* [8][ 9] */ { 1, 14 },
        /* [8][10] */ { 4,14 },
        /* [8][11] */ { 2,15 },
        /* [8][12] */ { 1, 15 },
        /* [8][13] */ { UNMAPPED },
        /* [8][14] */ { UNMAPPED },
        /* [8][15] */ { UNMAPPED },
    },
    // phys row 9
    {
        /* [9][ 0] */ { UNMAPPED },
        /* [9][ 1] */ { UNMAPPED },
        /* [9][ 2] */ { 2, 4 },
        /* [9][ 3] */ { 5, 4 },
        /* [9][ 4] */ { 5, 8 },
        /* [9][ 5] */ { UNMAPPED },
        /* [9][ 6] */ { UNMAPPED },
        /* [9][ 7] */ { 1, 9 },
        /* [9][ 8] */ { UNMAPPED },
        /* [9][ 9] */ { 2,13 },
        /* [9][10] */ { UNMAPPED },
        /* [9][11] */ { UNMAPPED },
        /* [9][12] */ { UNMAPPED },
        /* [9][13] */ { 1,16 },
        /* [9][14] */ { UNMAPPED },
        /* [9][15] */ { UNMAPPED },
    },
    // phys row 10
    {
        /* [10][ 0] */ { 5, 3 },
        /* [10][ 1] */ { UNMAPPED },
        /* [10][ 2] */ { 2, 3 },
        /* [10][ 3] */ { UNMAPPED },
        /* [10][ 4] */ { 5, 6 },
        /* [10][ 5] */ { UNMAPPED },
        /* [10][ 6] */ { UNMAPPED },
        /* [10][ 7] */ { 1, 8 },
        /* [10][ 8] */ { UNMAPPED },
        /* [10][ 9] */ { 2,12 },
        /* [10][10] */ { 5,10 },
        /* [10][11] */ { 5, 9 },
        /* [10][12] */ { 1, 17 },
        /* [10][13] */ { 2,16 },
        /* [10][14] */ { UNMAPPED },
        /* [10][15] */ { UNMAPPED },
    },
    // phys row 11
    {
        /* [11][ 0] */ { 4, 8 },
        /* [11][ 1] */ { 4, 9 },
        /* [11][ 2] */ { UNMAPPED },
        /* [11][ 3] */ { 4,10 },
        /* [11][ 4] */ { 4,11 },
        /* [11][ 5] */ { 3, 6 },
        /* [11][ 6] */ { 3, 8 },
        /* [11][ 7] */ { UNMAPPED },
        /* [11][ 8] */ { 3, 7 },
        /* [11][ 9] */ { 3, 9 },
        /* [11][10] */ { 3,10 },
        /* [11][11] */ { 4, 7 },
        /* [11][12] */ { 3,11 },
        /* [11][13] */ { 2,14 },
        /* [11][14] */ { UNMAPPED },
        /* [11][15] */ { 0,15 },
    },
};


//==============================================================================
// 2) DRIVER STATE
//==============================================================================
static matrix_row_t raw_phys[PHYS_ROWS];
static matrix_row_t logical_matrix[LOGIC_ROWS];
static matrix_row_t previous_matrix[LOGIC_ROWS];

//==============================================================================
// 3) PIN HELPERS & INIT (unchanged)
//==============================================================================

static inline void setPinOutput_writeLow(pin_t pin) {
    setPinOutput(pin);
    writePinLow(pin);
}

void matrix_init_custom(void) {
    // your original init:
    for (int i = 0; i <= 7; i++) {
        setPinInputHigh(i);
    }
    for (int i = 8; i <= 19; i++) {
        writePinLow(i);
        setPinInputHigh(i);
    }

    setPinInput(SHIFTREG_DATA);
    writePinLow(SHIFTREG_CLK);
    setPinOutput(SHIFTREG_CLK);
    writePinLow(SHIFTREG_SHLD);
    setPinOutput(SHIFTREG_SHLD);
}

//==============================================================================
// 4) SCAN + REMAP
//==============================================================================

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    // 4a) Physical scan → raw_phys[]
    for (int pr = 0; pr < PHYS_ROWS; pr++) {
        setPinOutput_writeLow(8 + pr);
        matrix_output_select_delay();

        writePinHigh(SHIFTREG_SHLD);
        uint8_t high_byte = palReadPort(PAL_PORT(GP0)) & 0xFF;
        setPinInputHigh(8 + pr);

        uint8_t lo = 0;
        for (int bit = 7; bit >= 0; bit--) {
            lo |= (!!readPin(SHIFTREG_DATA)) << bit;
            writePinHigh(SHIFTREG_CLK);
            wait_cpuclock(SHIFTREG_DELAY_CYCLES);
            writePinLow(SHIFTREG_CLK);
            wait_cpuclock(SHIFTREG_DELAY_CYCLES);
        }
        writePinLow(SHIFTREG_SHLD);

        raw_phys[pr] = ~(bitrev(lo) | (bitrev(high_byte) << 8));
        matrix_output_unselect_delay(pr, raw_phys[pr] != 0);
    }

    // 4b) Remap raw_phys → logical_matrix
    for (int lr = 0; lr < LOGIC_ROWS; lr++) {
        logical_matrix[lr] = 0;
    }
    for (int pr = 0; pr < PHYS_ROWS; pr++) {
        for (int pc = 0; pc < PHYS_COLS; pc++) {
            if (raw_phys[pr] & (1 << pc)) {
                uint8_t lr = phys2log[pr][pc][0];
                uint8_t lc = phys2log[pr][pc][1];
                if (lr < LOGIC_ROWS && lc < LOGIC_COLS) {
                    logical_matrix[lr] |= (1 << lc);
                }
            }
        }
    }

    // 4c) Change‐detect & write out
    bool changed = false;
    for (int lr = 0; lr < LOGIC_ROWS; lr++) {
        if (previous_matrix[lr] != logical_matrix[lr]) {
            changed = true;
            previous_matrix[lr] = logical_matrix[lr];
        }
        current_matrix[lr] = logical_matrix[lr];
    }
    return changed;
}
