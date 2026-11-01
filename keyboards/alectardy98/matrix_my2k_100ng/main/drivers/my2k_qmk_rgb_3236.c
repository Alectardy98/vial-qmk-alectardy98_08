/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Reuses the existing licensed QMK driver; no proprietary driver code. */
#define IS31FL3236_LED_COUNT 3
#define IS31FL3236_I2C_ADDRESS_1 0x3c
#define IS31FL3236_I2C_TIMEOUT 5
#include "my2k_rgb_i2c.h"
#include <string.h>
#define i2c_write_register my2k_rgb_i2c_write_register
#define i2c_addresses my2k_3236_i2c_addresses
#define driver_buffers my2k_3236_driver_buffers
#include "../../../drivers/led/issi/is31fl3236.c"
const is31fl3236_led_t PROGMEM g_is31fl3236_leds[3] = {
    {0, 30, 31, 32}, /* global 125 */
    {0, 15, 16, 17}, /* global 126 */
    {0, 18, 19, 20}, /* global 127 */
};
void my2k_qmk_rgb_3236_init(void) {
    memset(driver_buffers, 0, sizeof(driver_buffers));
    is31fl3236_init_drivers();
#if MY2K_DEV3_PROFILE
    is31fl3236_write_register(0, IS31FL3236_REG_GLOBAL_CONTROL, 0);
    is31fl3236_write_register(0, IS31FL3236_REG_UPDATE, 0);
#endif
}
void my2k_qmk_rgb_3236_set(unsigned index, uint8_t r, uint8_t g, uint8_t b) {
    is31fl3236_set_color((int)index, r, g, b);
}
void my2k_qmk_rgb_3236_flush(void) {
    is31fl3236_flush();
}
