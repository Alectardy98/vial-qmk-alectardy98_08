#include "timer.h"
#include "debug.h"
#include "matrix.h"
#include "print.h"
#include "ps2.h"

/*
 * IBM PS/2 raw-byte diagnostic mapper (ATmega32U4)
 *
 * Primary PS/2 bus only:
 *   DATA  = D0
 *   CLOCK = D1
 *
 * hid_listen output is intentionally only the raw valid PS/2 bytes,
 * for example: 15 F0 15 or E0 75 E0 F0 75
 */
static matrix_row_t matrix[MATRIX_ROWS];

__attribute__((weak)) void matrix_init_kb(void) { matrix_init_user(); }
__attribute__((weak)) void matrix_scan_kb(void) { matrix_scan_user(); }
__attribute__((weak)) void matrix_init_user(void) {}
__attribute__((weak)) void matrix_scan_user(void) {}

matrix_row_t matrix_get_row(uint8_t row) {
    return matrix[row];
}

void matrix_print(void) {}

void matrix_init(void) {
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
        matrix[i] = 0;
    }
    ps2_host_init();
    matrix_init_kb();
}

uint8_t matrix_scan(void) {
    static bool break_pending = false;
    uint8_t code = ps2_host_recv();


    if (code) {
        xprintf("RAW:%02X ", code);
        if (break_pending && code != 0xF0 && code != 0xE0) { xprintf("\n"); break_pending = false; }
        else if (code == 0xF0) { break_pending = true; }
    }

    matrix_scan_kb();
    return 0;
}
