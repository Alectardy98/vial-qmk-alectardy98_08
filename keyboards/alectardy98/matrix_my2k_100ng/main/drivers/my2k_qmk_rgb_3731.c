/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Reuses the existing licensed QMK driver; no proprietary driver code. */
#define IS31FL3731_LED_COUNT 59
#define IS31FL3731_I2C_ADDRESS_1 0x74
#define IS31FL3731_I2C_ADDRESS_2 0x77
#define IS31FL3731_I2C_ADDRESS_3 0x75
#define IS31FL3731_I2C_TIMEOUT 5
#include "my2k_rgb_i2c.h"
#include <string.h>
#define i2c_write_register my2k_rgb_i2c_write_register
#define i2c_addresses my2k_3731_i2c_addresses
#define driver_buffers my2k_3731_driver_buffers
#include "../../../drivers/led/issi/is31fl3731.c"
const is31fl3731_led_t PROGMEM g_is31fl3731_leds[59] = {
    {0, 0, 33, 49},     /* global 66 */
    {0, 1, 17, 50},     /* global 67 */
    {0, 2, 18, 34},     /* global 68 */
    {0, 3, 19, 35},     /* global 69 */
    {0, 4, 20, 36},     /* global 70 */
    {0, 5, 21, 37},     /* global 71 */
    {0, 6, 22, 38},     /* global 72 */
    {0, 7, 23, 39},     /* global 73 */
    {0, 128, 112, 96},  /* global 74 */
    {0, 129, 113, 97},  /* global 75 */
    {0, 130, 114, 98},  /* global 76 */
    {0, 131, 115, 99},  /* global 77 */
    {0, 132, 116, 100}, /* global 78 */
    {0, 133, 117, 101}, /* global 79 */
    {0, 8, 41, 57},     /* global 80 */
    {0, 9, 25, 58},     /* global 81 */
    {0, 10, 26, 42},    /* global 82 */
    {0, 11, 27, 43},    /* global 83 */
    {0, 12, 28, 44},    /* global 84 */
    {1, 141, 125, 109}, /* global 85 */
    {1, 140, 124, 108}, /* global 86 */
    {1, 139, 123, 107}, /* global 87 */
    {1, 138, 122, 106}, /* global 88 */
    {1, 137, 121, 105}, /* global 89 */
    {1, 136, 120, 104}, /* global 90 */
    {1, 15, 31, 47},    /* global 91 */
    {1, 14, 30, 46},    /* global 92 */
    {1, 13, 29, 45},    /* global 93 */
    {1, 12, 28, 44},    /* global 94 */
    {1, 11, 27, 43},    /* global 95 */
    {1, 10, 26, 42},    /* global 96 */
    {1, 9, 25, 58},     /* global 97 */
    {1, 8, 41, 57},     /* global 98 */
    {1, 4, 20, 36},     /* global 99 */
    {1, 3, 19, 35},     /* global 100 */
    {1, 2, 18, 34},     /* global 101 */
    {1, 1, 17, 50},     /* global 102 */
    {1, 0, 33, 49},     /* global 103 */
    {2, 0, 33, 49},     /* global 104 */
    {2, 1, 17, 50},     /* global 105 */
    {2, 2, 18, 34},     /* global 106 */
    {2, 7, 23, 39},     /* global 107 */
    {2, 6, 22, 38},     /* global 108 */
    {2, 3, 19, 35},     /* global 109 */
    {2, 4, 20, 36},     /* global 110 */
    {2, 5, 21, 37},     /* global 111 */
    {2, 128, 112, 96},  /* global 112 */
    {2, 9, 25, 58},     /* global 113 */
    {2, 10, 26, 42},    /* global 114 */
    {2, 11, 27, 43},    /* global 115 */
    {2, 12, 28, 44},    /* global 116 */
    {2, 13, 29, 45},    /* global 117 */
    {2, 14, 30, 46},    /* global 118 */
    {2, 15, 31, 47},    /* global 119 */
    {2, 136, 120, 104}, /* global 120 */
    {2, 138, 122, 106}, /* global 121 */
    {2, 139, 123, 107}, /* global 122 */
    {2, 140, 124, 108}, /* global 123 */
    {2, 141, 125, 109}, /* global 124 */
};
void my2k_qmk_rgb_3731_init(void) {
    memset(driver_buffers, 0, sizeof(driver_buffers));
#if MY2K_DEV3_PROFILE
    for (unsigned d = 0; d < IS31FL3731_DRIVER_COUNT; d++) {
        is31fl3731_select_page(d, IS31FL3731_COMMAND_FUNCTION);
        is31fl3731_write_register(d, 0x0a, 0); /* blank during setup */
        is31fl3731_write_register(d, 0x08, 0);
        is31fl3731_write_register(d, 0x09, 0); /* disable retained hardware breathing */
        is31fl3731_write_register(d, 0x0b, 0);
        is31fl3731_write_register(d, 0x0c, 0);    /* no audio gain/ADC modulation */
        is31fl3731_write_register(d, 0xc2, 0x10); /* stock080260C0 deghost setup */
    }
#endif
    is31fl3731_init_drivers();
}
void my2k_qmk_rgb_3731_set(unsigned index, uint8_t r, uint8_t g, uint8_t b) {
    is31fl3731_set_color((int)index, r, g, b);
}
void my2k_qmk_rgb_3731_flush(void) {
    is31fl3731_flush();
}
