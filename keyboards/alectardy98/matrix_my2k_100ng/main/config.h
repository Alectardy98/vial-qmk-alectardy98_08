// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#define MATRIX_ROWS 8
#define MATRIX_COLS 16
#define DEBOUNCE 5
#define USB_DRIVER USBD2
#define STM32_USE_USB_OTG2_HS FALSE
#define DYNAMIC_KEYMAP_LAYER_COUNT 4
#define EEPROM_SIZE 8192
#define EARLY_INIT_PERFORM_BOOTLOADER_JUMP FALSE
#define USB_POLLING_INTERVAL_MS 1
#define MY2K_PHYSICAL_WRITES_ENABLED 0

#define MY2K_USBH_RUNTIME_ENABLED 0
#define I2C_DRIVER I2CD1
#define I2C1_SCL_PIN B6
#define I2C1_SDA_PIN B7
#define I2C1_SCL_PAL_MODE 4
#define I2C1_SDA_PAL_MODE 4
#define I2C1_CLOCK_SPEED 400000
#define I2C1_DUTY_CYCLE FAST_DUTY_CYCLE_2

#define MY2K_UART_RUNTIME_ENABLED 0

#define MY2K_RGB_RUNTIME_ENABLED 0

/* Dedicated keyboard configuration block; native Vial keymaps/macros are
 * automatically placed after this block by QMK NVM. */
#define EECONFIG_KB_DATA_SIZE 592
#define EECONFIG_KB_DATA_VERSION 1
