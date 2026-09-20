/**
 * @file custom_matrix.c
 */

#include "matrix.h"
#include "wait.h"
#include "led.h"
#include "quantum.h"
#include "analog.h"


#ifndef CUSTOM_MATRIX_DEBUG
#define CUSTOM_MATRIX_DEBUG 1
#endif

#if CUSTOM_MATRIX_DEBUG
#define custom_matrix_debug  printf
#else
#define custom_matrix_debug(...)
#endif

#define COL_A_MASK  0x01
#define COL_B_MASK  0x02
#define COL_C_MASK  0x04
#define L_MASK      0x08
#define R_MASK      0x10

#define gpio_set_output_pushpull setPinOutputPushPull
// Removed: #define gpio_write_pin writePin

static uint32_t custom_row_pins[] = {ROW_6_PIN, ROW_1_PIN, ROW_2_PIN, ROW_3_PIN, ROW_4_PIN, ROW_5_PIN};
static uint8_t custom_col_pins[] = {L_MASK|3, L_MASK|0, L_MASK|1, L_MASK|2, L_MASK|4, L_MASK|6, L_MASK|7, L_MASK|5,
                                  R_MASK|3, R_MASK|0, R_MASK|1, R_MASK|2, R_MASK|4, R_MASK|6, R_MASK|7, R_MASK|5};

void matrix_init_custom(void)
{
    #ifdef CAPS_LED_PIN
    gpio_set_output_pushpull(CAPS_LED_PIN);
    palClearLine(CAPS_LED_PIN);
    #endif

    #ifdef RGB_EN_PIN
    gpio_set_output_pushpull(RGB_EN_PIN);
    palSetLine(RGB_EN_PIN);
    #endif

    //power on switch board
    #ifdef POWER_PIN
    gpio_set_output_pushpull(POWER_PIN);
    palSetLine(POWER_PIN);

    wait_ms(100);
    #endif

    // initialize row pins
    gpio_set_output_pushpull(ROW_6_PIN);
    palClearLine(ROW_6_PIN);
    gpio_set_output_pushpull(ROW_1_PIN);
    palClearLine(ROW_1_PIN);
    gpio_set_output_pushpull(ROW_2_PIN);
    palClearLine(ROW_2_PIN);
    gpio_set_output_pushpull(ROW_3_PIN);
    palClearLine(ROW_3_PIN);
    gpio_set_output_pushpull(ROW_4_PIN);
    palClearLine(ROW_4_PIN);
    gpio_set_output_pushpull(ROW_5_PIN);
    palClearLine(ROW_5_PIN);

    // initialize col pins
    gpio_set_output_pushpull(LEFT_EN_PIN);
    palSetLine(LEFT_EN_PIN);
    gpio_set_output_pushpull(RIGHT_EN_PIN);
    palSetLine(RIGHT_EN_PIN);

    gpio_set_output_pushpull(COL_A_PIN);
    palClearLine(COL_A_PIN);
    gpio_set_output_pushpull(COL_B_PIN);
    palClearLine(COL_B_PIN);
    gpio_set_output_pushpull(COL_C_PIN);
    palClearLine(COL_C_PIN);

    // initialize opamp and capacity discharge
    gpio_set_output_pushpull(OPA_EN_PIN);
    palSetLine(OPA_EN_PIN);
    gpio_set_output_pushpull(DISCHARGE_PIN);
    palClearLine(DISCHARGE_PIN);
}

static uint32_t matrix_adc_read(void)
{
    uint32_t data = 0;
    data = analogReadPin(A2);
    return data;
}

static uint32_t sense_key(pin_t row)
{
    palSetLine(DISCHARGE_PIN);
    wait_us(2);
    palClearLine(DISCHARGE_PIN);
    palSetLine(row);
    uint32_t data = matrix_adc_read();

    // clean up
    palClearLine(row);
    wait_us(20);

    return data;
}

// Update press/release state of key at (row, col)
bool ecsm_update_key(matrix_row_t* current_row, uint8_t col, uint16_t sw_value) {
    bool current_state = (*current_row >> col) & 1;

    // press to release
    if (current_state && sw_value < SENSE_LOW) {
        *current_row &= ~(1 << col);
        return true;
    }

    // release to press
    if ((!current_state) && sw_value > SENSE_HIGH) {
        *current_row |= (1 << col);
        return true;
    }

    return false;
}

uint32_t readings[MATRIX_ROWS][MATRIX_COLS] = { 0 };

bool matrix_scan_custom(matrix_row_t* raw)
{
    bool changed = false;

#if SCAN_ONE
    changed = scan_one(raw);
#else
    for (int col = 0; col < MATRIX_COLS; col++) {

        palWriteLine(COL_A_PIN, (bool)(custom_col_pins[col] & COL_A_MASK));
        palWriteLine(COL_B_PIN, (bool)(custom_col_pins[col] & COL_B_MASK));
        palWriteLine(COL_C_PIN, (bool)(custom_col_pins[col] & COL_C_MASK));

        if (custom_col_pins[col] & L_MASK) {
            palClearLine(LEFT_EN_PIN);
        }
        if (custom_col_pins[col] & R_MASK) {
            palClearLine(RIGHT_EN_PIN);
        }

        for (int row = 0; row < MATRIX_ROWS; row++) {
            readings[row][col] = sense_key(custom_row_pins[row]);
            changed |= ecsm_update_key(&raw[row], col, readings[row][col]);
        }
        palSetLine(LEFT_EN_PIN);
        palSetLine(RIGHT_EN_PIN);
    }
#endif

    // for (int row = 0; row < MATRIX_ROWS; ++row) {
    //     for (int col = 0; col < MATRIX_COLS; ++col) {
    //         printf("%6d ", readings[row][col]);
    //     }
    //     printf("\n");
    // }
    // printf("\n");

    // if (changed) {
    //     for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
    //         custom_matrix_debug("row:%d-%x\n", row, matrix_get_row(row));
    //     }
    // }
    return changed;
}

// void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
//     uint32_t offset = 0;
//     memcpy(&offset, data + 1, 4);

//     memcpy(data, (uint8_t*)readings + offset, 32);
// }

#define DBL_TAP_MAGIC    0xf01669ef
#define DBL_TAP_ADDR     ((volatile uint32_t *)0x2001FFF0)
#define DBL_TAP_DELAY_MS 500

void _board_dfu_dbl_tap(void) {
    uint32_t t0 = chVTGetSystemTimeX();
    uint32_t timeout = TIME_MS2I(DBL_TAP_DELAY_MS);

    if (*DBL_TAP_ADDR == DBL_TAP_MAGIC) {
        *DBL_TAP_ADDR = 0;
        __asm__ volatile ("bkpt #0"); // <-- For debugging, remove when done
        bootloader_jump();  // This calls TinyUF2 jump to bootloader
    } else {
        *DBL_TAP_ADDR = DBL_TAP_MAGIC;
        chThdSleep(timeout);
        *DBL_TAP_ADDR = 0;
    }
}

#include "bootloader.h"

__attribute__((weak))
void qk_bootmagic(void) {
    bootloader_jump();
    wait_ms(50);
    bootloader_jump();
}
