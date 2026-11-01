// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "wait.h"
#if MY2K_ACTIVE_DEV_PROFILE
#include "main_sleep.h"
#endif
#if MY2K_DEV4_PROFILE
#include "main_perf.h"
#if MY2K_DEV16_PROFILE
#include "main_input_performance.h"
#include "main_input_scheduler.h"
#endif
#endif
#if MY2K_DEV8_PROFILE
#include "main_button.h"
#include "timer.h"
#include <string.h>
static bool button_pressed;
static uint32_t button_samples,button_edges,button_sampled_at;
_Static_assert(MATRIX_ROWS>5 && MATRIX_COLS>3,"PB5 stock bridge needs row5,col3");
#endif
static const pin_t row_pins[MATRIX_ROWS] = {B11, B10, B2, B1, B0, A7, B8, B9};
static void write_row(uint8_t row, bool high) {
    /* Original 0802916e writes the complete 32-bit BSRR register. The generic
     * QMK set/clear macros select halfword accesses; use the recovered width
     * on this Geehy board without changing the generic GPIO implementation. */
    palWritePad(PAL_PORT(row_pins[row]), PAL_PAD(row_pins[row]), high);
}
void matrix_init_custom(void) {
#if MY2K_DEV8_PROFILE
    /* Stock080227F4 configures PB5 as an input with pull-up. Stock0802280C
     * injects that dedicated active-low switch into logical row5,col3. */
    gpio_set_pin_input_high(B5);
#endif
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        write_row(row, true);
        gpio_set_pin_output(row_pins[row]);
    }
    for (uint8_t col = 0; col < MATRIX_COLS; ++col)
        palSetPadMode(GPIOC, col, PAL_MODE_INPUT_PULLUP);
}
bool matrix_scan_custom(matrix_row_t current_matrix[]) {
#if MY2K_DEV4_PROFILE
    uint32_t started=my2k_perf_clock();
#if MY2K_DEV16_PROFILE
    my2k_input_scan(started);my2k_input_scheduler_scan(started);
#endif
#endif
    bool changed = false;
    bool pressed = false;
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        write_row(row, false);
        wait_us(5);
        matrix_row_t sample = (matrix_row_t)(~palReadPort(GPIOC) & 0xffffU);
        write_row(row, true);
#if MY2K_DEV8_PROFILE
        if(row==5){
            bool pressed=!gpio_read_pin(B5);
            if(pressed!=button_pressed)button_edges++;
            button_pressed=pressed;button_samples++;button_sampled_at=timer_read32();
            sample=(sample & ~((matrix_row_t)1<<3)) | (pressed?((matrix_row_t)1<<3):0);
        }
#endif
        changed |= current_matrix[row] != sample;
        current_matrix[row] = sample;
        pressed |= sample != 0;
    }
#if MY2K_ACTIVE_DEV_PROFILE
    /* Includes modifiers, KC_NO, held switches and the storage exit key.
     * Only observes the existing raw samples; never consumes an event. */
    if (pressed) my2k_sleep_activity(timer_read32());
#else
    (void)pressed;
#endif
#if MY2K_DEV4_PROFILE
    my2k_perf_record(MY2K_PERF_SCAN,started);
#endif
    return changed;
}

#if MY2K_DEV8_PROFILE
static void put32(uint8_t *p,uint32_t value){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(value>>(8*i));}
void my2k_button_status(uint8_t report[32]){
    memset(report+4,0,28);report[4]=1;report[5]=button_pressed;report[6]=5;report[7]=3;
    put32(report+8,button_samples);put32(report+12,button_edges);put32(report+16,button_sampled_at);
}
#endif
