#pragma once

/* Leyden Jar Rev 3; native PIO, DAC and IO-expander drivers. */
#define I2C_DRIVER I2CD0
#define I2C1_SDA_PIN GP16
#define I2C1_SCL_PIN GP17
#define I2C1_CLOCK_SPEED 400000
#define CONTROLLER_ROWS 8
#define CONTROLLER_COLS 18
#define LEYDEN_JAR_MATRIX_ROWS {7, 6, 5, 4, 3, 2, 1, 0}
#define LEYDEN_JAR_MATRIX_COLS {11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 255, 255, 255, 255, 255, 255}
#define LEYDEN_JAR_ACTIVE_PADS {0xbe, 0xff, 0x9f, 0xb7, 0xee, 0xff, 0xff, 0xff, 0xff, 0xff, 0xaf, 0xae, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}
#define NB_CAL_BINS 1
#define ACTIVATION_OFFSETS {10}
#define HAPTIC_ENABLE_PIN GP28
#define SOLENOID_PIN GP29
#define SOLENOID_DEFAULT_DWELL 20
#define SOLENOID_MIN_DWELL 4
#define SOLENOID_MAX_DWELL 100
#define HAPTIC_OFF_IN_LOW_POWER 1
#define USB_POLLING_INTERVAL_MS 1
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET
