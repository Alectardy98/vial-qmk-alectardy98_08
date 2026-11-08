// Copyright 2026 Alectardy98
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>

#ifdef XWHATSIT_RUNTIME_CALIBRATION
extern uint8_t xcal_settings[8];
void xcal_load(void);
void xcal_defaults(void);
void xcal_get_defaults(uint8_t *record);
uint8_t xcal_crc(const uint8_t *record);
uint8_t xcal_valid(const uint8_t *record);
void xcal_save(void);
void calibration(void);
#    define XCAL_SETTLE xcal_settings[1]
#    define XCAL_SAMPLE xcal_settings[2]
#    define XCAL_INIT_REPS (1U << (xcal_settings[3] & 15))
#    define XCAL_KEY_REPS (1U << (xcal_settings[3] >> 4))
#    define XCAL_BINS xcal_settings[4]
#    define XCAL_OFFSET ((uint16_t)xcal_settings[5] | ((uint16_t)xcal_settings[6] << 8))
#else
#    define XCAL_SETTLE CAPSENSE_DAC_SETTLE_TIME_US
#    define XCAL_SAMPLE CAPSENSE_HARDCODED_SAMPLE_TIME
#    define XCAL_INIT_REPS CAPSENSE_CAL_INIT_REPS
#    define XCAL_KEY_REPS CAPSENSE_CAL_EACHKEY_REPS
#    define XCAL_BINS CAPSENSE_CAL_BINS
#    define XCAL_OFFSET CAPSENSE_CAL_THRESHOLD_OFFSET
#endif
