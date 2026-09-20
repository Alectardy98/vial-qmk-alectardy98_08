#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include "wait.h"
#include "action_layer.h"
#include "print.h"
#include "debug.h"
#include "util.h"
#include "matrix.h"
#include "led.h"
#include <util/atomic.h>
#include "i2c_master.h"
#include "gpio.h"

/* Indicator LED (kept), solenoid pins handled by QMK haptics now */
#define INDICATOR     D4

/* I2C expander base addr */
#define TWI_BASE_ADDR 0x40

/* Example register addresses (adjust to your IO expander) */
#define IN_PORT_0   0x00
#define IN_PORT_1   0x01
#define OUT_PORT_0  0x02
#define OUT_PORT_1  0x03
#define POL_INV_0   0x04
#define POL_INV_1   0x05
#define CFG_PORT_0  0x06
#define CFG_PORT_1  0x07

extern matrix_row_t matrix[MATRIX_ROWS];

/* Physical/Logical dimensions (informational; QMK uses MATRIX_ROWS/COLS) */
#define LOGIC_ROWS 5
#define LOGIC_COLS 16

#define PHYS_ROWS  1
#define PHYS_COLS  66
#define PHYS_KEYS  (PHYS_ROWS * PHYS_COLS)

/* PHYS → LOGIC map
 * phys2log[phys_row][phys_col] = { logic_row (0..4), logic_col (0..15) }
 */
#define UNMAPPED 0xFF, 0xFF

static const uint8_t phys2log[PHYS_ROWS][PHYS_COLS][2] = {
    {   /* phys row 0 */
        /* 0..15  => logical row 0 */
        {0,0},{0,1},{0,2},{0,3},{0,4},{0,5},{0,6},{0,7},
        {0,8},{0,9},{0,10},{0,11},{0,12},{0,13},{0,14},{0,15},
        /* 16..31 => logical row 1 */
        {1,0},{1,1},{1,2},{1,3},{1,4},{1,5},{1,6},{1,7},
        {1,8},{1,9},{1,10},{1,11},{1,12},{1,13},{1,14},{1,15},
        /* 32..47 => logical row 2 */
        {2,0},{2,1},{2,2},{2,3},{2,4},{2,5},{2,6},{2,7},
        {2,8},{2,9},{2,10},{2,11},{2,12},{2,13},{2,14},{2,15},
        /* 48..62 => logical row 3 (15 keys) */
        {3,0},{3,1},{3,2},{3,3},{3,4},{3,5},{3,6},{3,7},
        {3,8},{3,9},{3,10},{3,11},{3,12},{3,13},{3,14},
        /* 63..65 => logical row 4 (3 keys) */
        {4,0},{4,1},{4,2},
    }
};

/* Local state */
static uint8_t prev_state[PHYS_KEYS];
static uint8_t curr_state[PHYS_KEYS];

/* Tiny I2C helper (write-then-read a register) */
static inline void i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len, uint16_t timeout_ms) {
    i2c_status_t st = i2c_transmit(addr, &reg, 1, timeout_ms);  // set register pointer
    if (st != I2C_STATUS_SUCCESS) return;
    (void)i2c_receive(addr, buf, len, timeout_ms);              // read N bytes
}

/* QMK hooks */
void matrix_init_user(void) {
    debug_enable = true;

    for (uint16_t i = 0; i < PHYS_KEYS; i++) {
        prev_state[i] = 0;
        curr_state[i] = 0;
    }

    /* Indicator LED only; solenoid is driven by QMK haptics */
    setPinOutput(INDICATOR);
    writePin(INDICATOR, 1);

    i2c_init();
}

void matrix_clear(void) {
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        matrix[r] = 0;
    }
}

/* Read one physical key bit from 5 slaves × 2 bytes each (pins[5][2]) */
static inline uint8_t read_phys_bit(uint16_t idx, uint8_t pins[5][2], uint8_t direct_bits) {
    if (idx < 80) {
        uint8_t block = idx / 8;
        uint8_t bit   = idx % 8;
        uint8_t slave = block / 2;        // two blocks per slave
        uint8_t hi    = block % 2;        // 0 -> [0], 1 -> [1]
        return (pins[slave][hi] >> bit) & 1;
    } else {
        uint8_t bit = idx - 80;
        return (direct_bits >> bit) & 1;
    }
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    uint8_t pins[5][2] = {{0}};
    uint8_t direct = PINF & 0b11;  /* two optional direct bits, if used */

    /* Read 2 input bytes from each of 5 I2C slaves, tight timeout (2ms) */
    for (uint8_t slave = 0; slave < 5; slave++) {
        i2c_read_reg((uint8_t)(TWI_BASE_ADDR | (slave << 1)), IN_PORT_0, pins[slave], 2, 2);
    }

    uint8_t any_change = 0;

    for (uint16_t p = 0; p < PHYS_KEYS; p++) {
        uint8_t last = prev_state[p];
        uint8_t now  = read_phys_bit(p, pins, direct);
        curr_state[p] = now;
        any_change   |= (last ^ now);
    }

    /* Build logical matrix from phys→logic map */
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        current_matrix[r] = 0;
    }
    for (uint16_t p = 0; p < PHYS_KEYS; p++) {
        uint8_t lr = phys2log[0][p][0];
        uint8_t lc = phys2log[0][p][1];
        if (lr == 0xFF || lc == 0xFF) continue;
        if (lr >= MATRIX_ROWS || lc >= MATRIX_COLS) continue;
        if (curr_state[p]) {
            current_matrix[lr] |= (matrix_row_t)(1u << lc);
        }
    }

    /* Move curr→prev for next scan */
    for (uint16_t p = 0; p < PHYS_KEYS; p++) {
        prev_state[p] = curr_state[p];
    }

    return any_change;
}
