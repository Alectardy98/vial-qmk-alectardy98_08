#pragma once

/* Versioned calibration preferences in QMK's keyboard EEPROM block.
 * Keymaps/macros keep the remainder of the existing 4 KiB logical EEPROM.
 */
#define EECONFIG_KB_DATA_SIZE 48
#define EECONFIG_KB_DATA_VERSION 0x4C4A0001
