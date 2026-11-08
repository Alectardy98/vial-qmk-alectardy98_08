// Copyright 2026 Alectardy98
// SPDX-License-Identifier: GPL-2.0-or-later
// Eight existing keyboard/user EEPROM bytes; no Vial address relocation.
#include "quantum.h"
#include "capsense.h"
#include "calibration_config.h"
#include "eeprom.h"
#include "quantum/nvm/eeprom/nvm_eeprom_eeconfig_internal.h"
#include <string.h>

#if EECONFIG_KB_DATA_SIZE || EECONFIG_USER_DATA_SIZE
#    error "xwhatsit calibration owns the fixed keyboard/user EEPROM words"
#endif
_Static_assert((uintptr_t)EECONFIG_USER == (uintptr_t)EECONFIG_KEYBOARD + 4, "EEPROM words must be adjacent");
_Static_assert(CAPSENSE_CAL_INIT_REPS && !(CAPSENSE_CAL_INIT_REPS & (CAPSENSE_CAL_INIT_REPS - 1)), "Initial repetitions must be a power of two");
_Static_assert(CAPSENSE_CAL_EACHKEY_REPS && !(CAPSENSE_CAL_EACHKEY_REPS & (CAPSENSE_CAL_EACHKEY_REPS - 1)), "Key repetitions must be a power of two");
_Static_assert(CAPSENSE_CAL_INIT_REPS <= 32 && CAPSENSE_CAL_EACHKEY_REPS <= 32, "Runtime calibration averaging is bounded to 32 samples");

uint8_t xcal_settings[8];

uint8_t xcal_crc(const uint8_t *record) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < 7; ++i) {
        crc ^= record[i];
        for (uint8_t b = 0; b < 8; ++b) crc = (crc & 0x80) ? (crc << 1) ^ 7 : crc << 1;
    }
    return crc;
}

uint8_t xcal_valid(const uint8_t *p) {
    // Subtract before the unsigned comparison to check both bounds at once;
    // averaging nibbles are exponents 0..5 (1..32 measurements).
    return p[0] == 0xA7 && (uint8_t)(p[1] - 1) < 32 && (uint8_t)(p[2] - 1) < 32 &&
           (p[3] & 15) <= 5 && (p[3] >> 4) <= 5 && (uint8_t)(p[4] - 1) < CAPSENSE_CAL_BINS &&
           ((uint16_t)p[5] | ((uint16_t)p[6] << 8)) <= CAPSENSE_DAC_MAX && p[7] == xcal_crc(p);
}

void xcal_get_defaults(uint8_t *record) {
    static const uint8_t PROGMEM defaults[7] = {0xA7, CAPSENSE_DAC_SETTLE_TIME_US, CAPSENSE_HARDCODED_SAMPLE_TIME,
        __builtin_ctz(CAPSENSE_CAL_INIT_REPS) | (__builtin_ctz(CAPSENSE_CAL_EACHKEY_REPS) << 4),
        CAPSENSE_CAL_BINS, CAPSENSE_CAL_THRESHOLD_OFFSET & 255, CAPSENSE_CAL_THRESHOLD_OFFSET >> 8};
    memcpy_P(record, defaults, 7);
    record[7] = xcal_crc(record);
}

void xcal_defaults(void) { xcal_get_defaults(xcal_settings); }

void xcal_load(void) {
    eeprom_read_block(xcal_settings, EECONFIG_KEYBOARD, 8);
    if (!xcal_valid(xcal_settings)) xcal_defaults();
}

void xcal_save(void) {
    uint8_t *slot = (uint8_t *)EECONFIG_KEYBOARD;
    // Commit marker last: interruption leaves either a complete record or an
    // invalid record that loads the compiled defaults on the next boot.
    eeprom_update_byte(slot, 0);
    eeprom_update_block(xcal_settings + 1, slot + 1, 7);
    eeprom_update_byte(slot, xcal_settings[0]);
}
