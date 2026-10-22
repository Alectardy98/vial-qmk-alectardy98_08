/* Modified for Alectardy98 EC42; original notices retained.
 * Modification attribution reviewed 2026-10-21; see THIRD_PARTY_NOTICES.md. */
/* Copyright 2023 Cipulot
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "matrix.h"
#include "eeconfig.h"
#include "util.h"
#include "ec42_protocol.h"

typedef struct PACKED {
    uint8_t  actuation_mode;                              // 0: fixed Variable Actuation, 1: VA-first Dynamic Rapid Trigger
    uint16_t mode_0_actuation_threshold;                  // threshold for key press in mode 0
    uint16_t mode_0_release_threshold;                    // threshold for key release in mode 0
    uint16_t mode_1_initial_deadzone_offset;              // threshold for key press in mode 1
    uint8_t  mode_1_actuation_offset;                     // raw ADC movement for dynamic reactivation (1-255)
    uint8_t  mode_1_release_offset;                       // raw ADC movement for dynamic release (1-255)
    uint16_t bottoming_reading[MATRIX_ROWS][MATRIX_COLS]; // bottoming reading
    uint16_t metadata[4]; // magic, version/mode, CRC low/high; outside the matrix
    uint8_t overrides[MATRIX_ROWS*MATRIX_COLS]; // 0 inherits; 1..98 %, 99 deepest
} eeprom_ec_config_t;

typedef struct {
    uint8_t  actuation_mode;                                                    // 0: normal board-wide APC, 1: Rapid trigger from specific board-wide actuation point (it can be very near that baseline noise and be "full travel")
    uint16_t mode_0_actuation_threshold;                                        // threshold for key press in mode 0
    uint16_t mode_0_release_threshold;                                          // threshold for key release in mode 0
    uint16_t mode_1_initial_deadzone_offset;                                    // threshold for key press in mode 1 (initial deadzone)
    uint8_t  mode_1_actuation_offset;                                           // offset for key press in mode 1 (1-255)
    uint8_t  mode_1_release_offset;                                             // offset for key release in mode 1 (1-255)
    uint16_t rescaled_mode_0_actuation_threshold[MATRIX_ROWS][MATRIX_COLS];     // threshold for key press in mode 0 rescaled to actual scale
    uint16_t rescaled_mode_0_release_threshold[MATRIX_ROWS][MATRIX_COLS];       // threshold for key release in mode 0 rescaled to actual scale
    uint16_t extremum[MATRIX_ROWS][MATRIX_COLS];                                // extremum values for mode 1
    uint16_t noise_floor[MATRIX_ROWS][MATRIX_COLS];                             // noise floor detected during startup
    bool     bottoming_calibration;                                             // calibration mode for bottoming out values (true: calibration mode, false: normal mode)
    uint16_t bottoming_reading[MATRIX_ROWS][MATRIX_COLS];                       // bottoming reading
} ec_config_t;

// Physical allocation can be split, but the record format is always 17+3*N.
_Static_assert(sizeof(eeprom_ec_config_t)==EC42_RECORD_BYTES(MATRIX_ROWS,MATRIX_COLS),"Canonical record size");
#ifndef EC_RECORD_SPLIT
_Static_assert(sizeof(eeprom_ec_config_t)==EECONFIG_KB_DATA_SIZE,"EC allocation size");
#endif

extern eeprom_ec_config_t eeprom_ec_config;

extern ec_config_t ec_config;

void init_row(void);
void init_amux(void);
void disable_unused_row(uint8_t row);
void select_amux_channel(uint8_t channel, uint8_t col);
void disable_unused_amux(uint8_t channel);
void discharge_capacitor(void);
void charge_capacitor(uint8_t row);

int      ec_init(void);
void     ec_noise_floor(void);
bool     ec_matrix_scan(matrix_row_t current_matrix[]);
uint16_t ec_readkey_raw(uint8_t channel, uint8_t row, uint8_t col);
bool     ec_update_key(matrix_row_t* current_row, uint8_t row, uint8_t col, uint16_t sw_value);
void     ec_print_matrix(void);

uint16_t rescale(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max);

bool is_unused_position(uint8_t row, uint8_t col);

#ifdef SPLIT_KEYBOARD
void via_cmd_slave_handler(uint8_t m2s_size, const void* m2s_buffer, uint8_t s2m_size, void* s2m_buffer);
#endif

// Portable hardware boundary: sample once, then shared capture/VA/RT/telemetry.
bool ec_process_sample(matrix_row_t *rows,uint8_t row,uint8_t col,uint16_t raw);
