#pragma once

/* Leyden Jar Rev 3; native PIO, DAC and IO-expander drivers. */
#define I2C_DRIVER I2CD0
#define I2C1_SDA_PIN GP16
#define I2C1_SCL_PIN GP17
#define I2C1_CLOCK_SPEED 400000
#define CONTROLLER_ROWS 8
#define CONTROLLER_COLS 18
#define LEYDEN_JAR_MATRIX_ROWS {7, 6, 5, 4, 3, 2, 1, 0}
#define LEYDEN_JAR_MATRIX_COLS {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 255, 255, 255}
#define LEYDEN_JAR_ACTIVE_PADS {0x3e, 0x3e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbf, 0xff, 0xaf, 0xff, 0xeb, 0x0, 0x0, 0x0}
/* Controller coordinates, not QMK coordinates or key assignments:
 * - Bin 0: Page Up position, released ~414; an earlier hold read 436..437.
 * - Bin 1: Tab, 7 and Page Down positions. Their held signals exceed 480,
 *   but neighbouring activity has produced readings up to 446.
 * - Bin 2: coupled 3/R positions; bin 3: coupled numpad 8/9 positions.
 *   These pairs must not share a reference: the numpad's released level
 *   shifted above 450 while 3/R remained near 410 in the assembled keyboard.
 * - Bin 4: L (2,7) and Left Arrow (0,10): released 409..411, held
 *   426..428 / 425..427. Their +8 reference avoids the old 427..428 thresholds.
 * - Bins 5..9: the remaining pads, grouped by released signal range rather
 *   than equal key counts. Each uses the group's highest released level.
 * Keep every custom-layout pad. See test_f107_calibration.py for capture
 * replay, coupling bounds, threshold boundaries and persistent defaults.
 */
#define LEYDEN_JAR_LEVEL_SCAN_MAX 1024
#define LEYDEN_JAR_RANGE_CALIBRATION
#define NB_CAL_BINS 10
#define NB_CUSTOM_CAL_BINS 5
#define CUSTOM_CAL_BIN_KEYS { \
    {.bin = 0, .row = 7, .col = 11}, \
    {.bin = 1, .row = 5, .col = 2}, \
    {.bin = 1, .row = 6, .col = 6}, \
    {.bin = 1, .row = 5, .col = 11}, \
    {.bin = 2, .row = 5, .col = 4}, \
    {.bin = 2, .row = 6, .col = 4}, \
    {.bin = 3, .row = 5, .col = 13}, \
    {.bin = 3, .row = 6, .col = 13}, \
    {.bin = 4, .row = 2, .col = 7}, \
    {.bin = 4, .row = 0, .col = 10} \
}
#define ACTIVATION_OFFSETS {12, 32, 35, 35, 8, 16, 16, 16, 16, 16}
#define HAPTIC_ENABLE_PIN GP28
#define SOLENOID_PIN GP29
#define SOLENOID_DEFAULT_DWELL 20
#define SOLENOID_MIN_DWELL 4
#define SOLENOID_MAX_DWELL 100
#define HAPTIC_OFF_IN_LOW_POWER 1
#define USB_POLLING_INTERVAL_MS 1
/* Normal scans take about 9 ms. Require more than one scan interval of
 * settling on each key; short dropouts must not split a continuous hold.
 * This is temporal filtering, not a substitute for correct calibration. */
#define DEBOUNCE 12
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET
