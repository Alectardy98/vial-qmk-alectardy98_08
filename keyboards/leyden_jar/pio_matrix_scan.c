/* Copyright 2022 Eric Becourt (Rico https://mymakercorner.com)
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

#include "quantum.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pio_matrix_scan.h"
#include "col_0_7_pio.pio.h"
#include "col_8_15_pio.pio.h"
#include "col_16_17_pio.pio.h"
#ifdef LEYDEN_JAR_GPIO_COLUMNS
#include "hardware/structs/timer.h"
#endif

const uint8_t s_TxColScanData[16] = {
    0x01, 0x00,
    0x02, 0x00,
    0x04, 0x00,
    0x08, 0x00,
    0x10, 0x00,
    0x20, 0x00,
    0x40, 0x00,
    0x80, 0x00,
};

const uint32_t s_TxCol2ScanData = ((1 << 0) | (2 << 4));

#ifdef LEYDEN_JAR_GPIO_COLUMNS
_Static_assert(CONTROLLER_ROWS == 4 && CONTROLLER_COLS == 20, "GPIO4-7 are outputs on the four-row 3276");
static uint8_t s_RawMatrixScanValues[20];
static bool s_gpio_columns_ready;
static uint32_t s_scans, s_errors, s_last_us, s_max_us;
#else
static uint8_t s_RawMatrixScanValues[18];
#endif
static bool s_enable_extra_cols;

void pio_matrix_scan_init(bool enable_extra_cols) {
    s_enable_extra_cols = enable_extra_cols;

    /* Get PIO0 peripheral out of reset state. */
    hal_lld_peripheral_unreset(RESETS_ALLREG_PIO0);

    /* Configuration of column pins using ChibiOS hal */

    iomode_t colums_pin_mode = PAL_RP_GPIO_OE | PAL_RP_PAD_SLEWFAST | PAL_RP_PAD_DRIVE8 | PAL_MODE_ALTERNATE_PIO0;
    for (int i=0; i<8; i++) {
        palSetLineMode(i+8, colums_pin_mode);
        wait_us(50);
        palSetLineMode(i+18, colums_pin_mode);
        wait_us(50);
    }

    if (s_enable_extra_cols == true) {
#ifndef LEYDEN_JAR_GPIO_COLUMNS
        palSetLineMode(26, colums_pin_mode);
        wait_us(50);
        palSetLineMode(27, colums_pin_mode);
        wait_us(50);
#endif
    }

    /* Configuration of row pins using ChibiOS hal */

    iomode_t rows_pin_mode = PAL_RP_PAD_IE | PAL_RP_PAD_SCHMITT | PAL_MODE_ALTERNATE_PIO0;
    for (int i=0; i<8; i++) {
        palSetLineMode(i, rows_pin_mode);
    }

    /* Load and start PIO programs */

    uint32_t offset0 = 0;
    uint32_t offset1 = 0;
    uint32_t offset2 = 0;

    offset0 = pio_add_program(pio0, &col_0_7_pio_program);
    offset1 = pio_add_program(pio0, &col_8_15_pio_program);
    if (s_enable_extra_cols == true) {
#ifdef LEYDEN_JAR_GPIO_COLUMNS
        /* Four GPIO4-7 drive pulses follow the sixteen native columns.
         * Preserve the native pulse and recovery timing. IRQ3 marks completion. */
        uint16_t instructions[col_16_17_pio_wrap + 2];
        memcpy(instructions, col_16_17_pio_program_instructions, sizeof(col_16_17_pio_program_instructions));
        instructions[1] = pio_encode_set(pio_x, 3);
        instructions[2] = pio_encode_out(pio_pins, 4) | pio_encode_delay(8);
        instructions[3] = pio_encode_in(pio_pins, 4) | pio_encode_delay(27);
        instructions[6] = pio_encode_out(pio_pins, 4) | pio_encode_delay(31);
        instructions[col_16_17_pio_wrap + 1] = pio_encode_irq_set(false, 3);
        const struct pio_program program = {.instructions = instructions, .length = ARRAY_SIZE(instructions), .origin = -1};
        offset2 = pio_add_program(pio0, &program);
#else
        offset2 = pio_add_program(pio0, &col_16_17_pio_program);
#endif
    }

    col_0_7_pio_init(pio0, 0, offset0, 0, 8);
    col_8_15_pio_init(pio0, 1, offset1, 0, 18);
    if (s_enable_extra_cols == true) {
#ifdef LEYDEN_JAR_GPIO_COLUMNS
        for (unsigned pin = 4; pin <= 7; ++pin) {
            palSetLineMode(pin, colums_pin_mode);
        }
        pio_sm_config config = col_16_17_pio_program_get_default_config(offset2);
        sm_config_set_wrap(&config, offset2, offset2 + col_16_17_pio_wrap + 1);
        sm_config_set_out_pins(&config, 4, 4);
        sm_config_set_in_pins(&config, 0);
        sm_config_set_out_shift(&config, true, true, 32);
        sm_config_set_in_shift(&config, true, true, 16);
        sm_config_set_clkdiv(&config, (float)clock_get_hz(clk_sys) / 10000000.0f);
        pio_sm_set_consecutive_pindirs(pio0, 2, 4, 4, true);
        pio_sm_init(pio0, 2, offset2, &config);
        s_gpio_columns_ready = true;
#else
        col_16_17_pio_init(pio0, 2, offset2, 0, 26);
#endif
    }

    pio_sm_set_enabled(pio0, 0, true);
    pio_sm_set_enabled(pio0, 1, true);
    if (s_enable_extra_cols == true) {
        pio_sm_set_enabled(pio0, 2, true);
    }
}

void pio_raw_scan() {
#ifdef LEYDEN_JAR_GPIO_COLUMNS
    if (!s_gpio_columns_ready) {
        return;
    }
    uint32_t start = timer_hw->timerawl;
    pio_interrupt_clear(pio0, 3);
#endif
    /* Queue the final drive group before starting the SM0 -> SM1 chain. */
    if (s_enable_extra_cols == true) {

#ifdef LEYDEN_JAR_GPIO_COLUMNS
        pio_sm_put_blocking(pio0, 2, 0x08040201u);
#else
        pio_sm_put_blocking(pio0, 2, s_TxCol2ScanData);
#endif
    }

    /* We then push FIFO data to the PIO state machine that handles column 8 to 15,
     that is the state machine that runs the col_8_15_pio program.
     As this program waits col_0_7_pio to finish no data will be immediately consumed. */

    pio_sm_put_blocking(pio0, 1, *(uint32_t*)(s_TxColScanData + 0));
    pio_sm_put_blocking(pio0, 1, *(uint32_t*)(s_TxColScanData + 4));
    pio_sm_put_blocking(pio0, 1, *(uint32_t*)(s_TxColScanData + 8));
    pio_sm_put_blocking(pio0, 1, *(uint32_t*)(s_TxColScanData + 12));

    /* We last push FIFO data to the PIO state machine that handles column 0 to 7,
     that is the state machine that runs the col_0_7_pio program.
     As soon as data is pushed the program will resume it's execution and consume data.
     This program will awaken the col_8_15_pio program when all columns will be scanned. */

    pio_sm_put_blocking(pio0, 0, *(uint32_t*)(s_TxColScanData + 0));
    pio_sm_put_blocking(pio0, 0, *(uint32_t*)(s_TxColScanData + 4));
    pio_sm_put_blocking(pio0, 0, *(uint32_t*)(s_TxColScanData + 8));
    pio_sm_put_blocking(pio0, 0, *(uint32_t*)(s_TxColScanData + 12));

    /* Match connector order. GPIO4-7 occupy drive channels 0-3 and the
     * native sixteen outputs occupy channels 4-19 in this adapter. */
#ifdef LEYDEN_JAR_GPIO_COLUMNS
    uint8_t *native_values = s_RawMatrixScanValues + 4;
#else
    uint8_t *native_values = s_RawMatrixScanValues;
#endif
    for (unsigned sm = 0; sm < 2; ++sm) {
        for (unsigned word = 0; word < 2; ++word) {
            uint32_t packed = pio_sm_get_blocking(pio0, sm);
            memcpy(native_values + sm * 8 + word * 4, &packed, sizeof(packed));
        }
    }

    // Read results send by col_16_17_pio program to the FIFO
    if (s_enable_extra_cols == true) {
#ifdef LEYDEN_JAR_GPIO_COLUMNS
        /* Wait for the fourth pulse and its recovery before changing DAC. */
        while (!pio_interrupt_get(pio0, 3) && timer_hw->timerawl - start < 2000) {
            wait_us(1);
        }
        if (!pio_interrupt_get(pio0, 3) || pio_sm_is_rx_fifo_empty(pio0, 2)) {
            pio_sm_set_enabled(pio0, 2, false);
            pio_sm_set_pins_with_mask(pio0, 2, 0, 0x0fu << 4);
            memset(s_RawMatrixScanValues, 0xff, sizeof(s_RawMatrixScanValues));
            s_gpio_columns_ready = false;
            ++s_errors;
            return;
        }
        uint32_t packed = pio_sm_get(pio0, 2);
        for (unsigned col = 0; col < 4; ++col) {
            s_RawMatrixScanValues[col] = (packed >> (16 + 4 * col)) & 0x0f;
        }
        s_last_us = timer_hw->timerawl - start;
        if (s_last_us > s_max_us) {
            s_max_us = s_last_us;
        }
        ++s_scans;
#else
        *(uint16_t*)(s_RawMatrixScanValues + 16) = (uint16_t)(pio_sm_get_blocking(pio0, 2)>>16);
#endif
    }
}

const uint8_t* pio_get_scan_vals(void) {
    return s_RawMatrixScanValues;
}

#ifdef LEYDEN_JAR_GPIO_COLUMNS
bool pio_gpio_columns_ready(void) {
    return s_gpio_columns_ready;
}

void pio_gpio_columns_status(uint8_t payload[28]) {
    memset(payload, 0, 28);
    payload[0] = 1;
    payload[1] = s_gpio_columns_ready;
    payload[2] = s_RawMatrixScanValues[0];
    payload[3] = s_RawMatrixScanValues[1];
    memcpy(payload + 4, &s_scans, sizeof(s_scans));
    memcpy(payload + 8, &s_errors, sizeof(s_errors));
    memcpy(payload + 12, &s_last_us, sizeof(s_last_us));
    memcpy(payload + 16, &s_max_us, sizeof(s_max_us));
    payload[20] = 4;
    payload[21] = 7;
    payload[22] = LEYDEN_JAR_IO_EXPANDER_INPUT_MASK;
    payload[23] = 4; // Four GPIO drives plus sixteen native column outputs.
}
#endif
