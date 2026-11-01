/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Reuses the existing licensed QMK driver; no proprietary driver code. */
#define IS31FL3729_LED_COUNT 66
#define IS31FL3729_I2C_ADDRESS_1 0x37
#define IS31FL3729_I2C_ADDRESS_2 0x34
#define IS31FL3729_I2C_TIMEOUT 5
#if MY2K_DEV_PROFILE
#    define IS31FL3729_CONFIGURATION 0x00 /* Stay shut down while clearing retained PWM. */
#    if MY2K_STOCK_PARITY_PROFILE
/* Exact original0802650C write, also identical in newest manufacturerUF2.
 * Do not replace the stock CSscale40 or3236control07 with maximum values. */
#        define IS31FL3729_GLOBAL_CURRENT 0xff
#    elif MY2K_DEV3_PROFILE
#        define IS31FL3729_GLOBAL_CURRENT 0x20 /*32/64; PWM<=16 and scaling64/256 remain bounded */
#    else
#        define IS31FL3729_GLOBAL_CURRENT 0x08 /* 8/64 global current; PWM capped separately */
#    endif
#else
#    define IS31FL3729_CONFIGURATION 0x01
#    define IS31FL3729_GLOBAL_CURRENT 0x00 /* conservative profiles remain dark */
#endif
#include "my2k_rgb_i2c.h"
#include <string.h>
#define i2c_write_register my2k_rgb_i2c_write_register
#define i2c_addresses my2k_3729_i2c_addresses
#define driver_buffers my2k_3729_driver_buffers
#include "../../../drivers/led/issi/is31fl3729.c"
const is31fl3729_led_t PROGMEM g_is31fl3729_leds[66] = {
    {1, 13, 12, 14},    /* global 0 */
    {1, 29, 28, 30},    /* global 1 */
    {1, 45, 44, 46},    /* global 2 */
    {1, 61, 60, 62},    /* global 3 */
    {1, 77, 76, 78},    /* global 4 */
    {1, 93, 92, 94},    /* global 5 */
    {1, 109, 108, 110}, /* global 6 */
    {1, 125, 124, 126}, /* global 7 */
    {0, 13, 12, 14},    /* global 8 */
    {0, 29, 28, 30},    /* global 9 */
    {0, 45, 44, 46},    /* global 10 */
    {0, 61, 60, 62},    /* global 11 */
    {0, 77, 76, 78},    /* global 12 */
    {0, 93, 92, 94},    /* global 13 */
    {0, 109, 108, 110}, /* global 14 */
    {0, 125, 124, 126}, /* global 15 */
    {1, 10, 9, 11},     /* global 16 */
    {1, 26, 25, 27},    /* global 17 */
    {1, 42, 41, 43},    /* global 18 */
    {1, 58, 57, 59},    /* global 19 */
    {1, 74, 73, 75},    /* global 20 */
    {1, 90, 89, 91},    /* global 21 */
    {1, 106, 105, 107}, /* global 22 */
    {1, 122, 121, 123}, /* global 23 */
    {0, 10, 9, 11},     /* global 24 */
    {0, 26, 25, 27},    /* global 25 */
    {0, 42, 41, 43},    /* global 26 */
    {0, 58, 57, 59},    /* global 27 */
    {0, 74, 73, 75},    /* global 28 */
    {0, 90, 89, 91},    /* global 29 */
    {0, 106, 105, 107}, /* global 30 */
    {0, 122, 121, 123}, /* global 31 */
    {1, 7, 6, 8},       /* global 32 */
    {1, 23, 22, 24},    /* global 33 */
    {1, 39, 38, 40},    /* global 34 */
    {1, 55, 54, 56},    /* global 35 */
    {1, 71, 70, 72},    /* global 36 */
    {1, 87, 86, 88},    /* global 37 */
    {1, 103, 102, 104}, /* global 38 */
    {1, 119, 118, 120}, /* global 39 */
    {0, 7, 6, 8},       /* global 40 */
    {0, 23, 22, 24},    /* global 41 */
    {0, 39, 38, 40},    /* global 42 */
    {0, 55, 54, 56},    /* global 43 */
    {0, 71, 70, 72},    /* global 44 */
    {0, 87, 86, 88},    /* global 45 */
    {0, 103, 102, 104}, /* global 46 */
    {0, 119, 118, 120}, /* global 47 */
    {1, 4, 3, 5},       /* global 48 */
    {1, 20, 19, 21},    /* global 49 */
    {1, 36, 35, 37},    /* global 50 */
    {1, 52, 51, 53},    /* global 51 */
    {1, 68, 67, 69},    /* global 52 */
    {1, 84, 83, 85},    /* global 53 */
    {1, 100, 99, 101},  /* global 54 */
    {1, 116, 115, 117}, /* global 55 */
    {0, 4, 3, 5},       /* global 56 */
    {0, 20, 19, 21},    /* global 57 */
    {0, 36, 35, 37},    /* global 58 */
    {0, 52, 51, 53},    /* global 59 */
    {0, 68, 67, 69},    /* global 60 */
    {0, 84, 83, 85},    /* global 61 */
    {0, 100, 99, 101},  /* global 62 */
    {0, 116, 115, 117}, /* global 63 */
    {1, 1, 0, 2},       /* global 64 */
    {1, 17, 16, 18},    /* global 65 */
};
void my2k_qmk_rgb_3729_flush(void) {
    for (unsigned d = 0; d < IS31FL3729_DRIVER_COUNT; d++)
        if (driver_buffers[d].pwm_buffer_dirty) {
#if MY2K_DEV3_PROFILE
            /*15x9 mode: match stock row transactions; never cross CS16/reserved slots. */
            for (unsigned row = 0; row < 9; row++)
                (void)i2c_write_register(i2c_addresses[d] << 1, 1 + 16 * row, driver_buffers[d].pwm_buffer + 16 * row,
                                         15, IS31FL3729_I2C_TIMEOUT);
#else
            for (unsigned offset = 0; offset < IS31FL3729_PWM_REGISTER_COUNT; offset += 13) {
                unsigned count = IS31FL3729_PWM_REGISTER_COUNT - offset;
                if (count > 13)
                    count = 13;
                (void)i2c_write_register(i2c_addresses[d] << 1, IS31FL3729_REG_PWM + offset,
                                         driver_buffers[d].pwm_buffer + offset, count, IS31FL3729_I2C_TIMEOUT);
            }
#endif
            driver_buffers[d].pwm_buffer_dirty = false;
        }
}
void my2k_qmk_rgb_3729_init(void) {
    memset(driver_buffers, 0, sizeof(driver_buffers));
    for (unsigned d = 0; d < IS31FL3729_DRIVER_COUNT; d++) {
#if MY2K_DEV3_PROFILE
        /* Manufacturer0802650C resets with CF=AE before configuration. */
        is31fl3729_write_register(d, IS31FL3729_REG_RESET, 0xae);
        wait_ms(10);
#endif
        is31fl3729_init(d);
        for (unsigned cs = 0; cs < IS31FL3729_SCALING_REGISTER_COUNT; cs++)
            driver_buffers[d].scaling_buffer[cs] = 0x40;
        driver_buffers[d].scaling_buffer_dirty = true;
        driver_buffers[d].pwm_buffer_dirty     = true;
#if MY2K_DEV3_PROFILE
        for (unsigned cs = 0; cs < 15; cs++)
            is31fl3729_write_register(d, IS31FL3729_REG_SCALING + cs, 0x40);
        driver_buffers[d].scaling_buffer_dirty = false;
#else
        is31fl3729_update_scaling_registers(d);
#endif
    }
    my2k_qmk_rgb_3729_flush();
#if MY2K_DEV_PROFILE
    /* Enable only after known-zero PWM and conservative scaling are installed. */
    for (unsigned d = 0; d < IS31FL3729_DRIVER_COUNT; d++)
        is31fl3729_write_register(d, IS31FL3729_REG_CONFIGURATION, 0x01);
#endif
}
void my2k_qmk_rgb_3729_set(unsigned index, uint8_t r, uint8_t g, uint8_t b) {
    is31fl3729_set_color((int)index, r, g, b);
}
