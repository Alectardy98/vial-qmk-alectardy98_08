#pragma once

/* Leyden Jar Rev 3; native PIO, DAC and IO-expander drivers. */
#define I2C_DRIVER I2CD0
#define I2C1_SDA_PIN GP16
#define I2C1_SCL_PIN GP17
#define I2C1_CLOCK_SPEED 400000
#define CONTROLLER_ROWS 4
#define CONTROLLER_COLS 20
/* GPIO4-7 become column outputs after U6 is removed. */
#define LEYDEN_JAR_GPIO_COLUMNS
/* Complete USB configuration before calibration begins. */
#define USB_WAIT_FOR_ENUMERATION
/* Leave the unused J2 LED0/LED1 outputs high-impedance. */
#define LEYDEN_JAR_IO_EXPANDER_INPUT_MASK 0x60
#define LEYDEN_JAR_MATRIX_ROWS {3, 2, 1, 0, 255, 255, 255, 255}
#define LEYDEN_JAR_MATRIX_COLS {19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0}
#define LEYDEN_JAR_ACTIVE_PADS {0xf, 0xf, 0x3, 0xe, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xe, 0x7, 0xf, 0xf}
#include "calibration.h"
#define BEAMSPRING_KEYBOARD
#define HAPTIC_ENABLE_PIN GP28
#define SOLENOID_PIN GP29
#define SOLENOID_DEFAULT_DWELL 20
#define SOLENOID_MIN_DWELL 4
#define SOLENOID_MAX_DWELL 100
#define HAPTIC_OFF_IN_LOW_POWER 1
#define USB_POLLING_INTERVAL_MS 1
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET
