// SPDX-License-Identifier: GPL-3.0-or-later
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
/* Adapted ec_update_key from Cipulot common/ec_switch_matrix.c; EC42 adds calibrated-key suppression, shared dispatch and VA-first RT.
 * Modified for the EC42 framework; attribution updated 2026-10-21. */
#include "ec_switch_matrix.h"
#include "calibration.h"
#include "print.h"
#include "ec_diagnostic.h"
bool ec_update_key(matrix_row_t* current_row, uint8_t row, uint8_t col, uint16_t sw_value) {
    bool current_state = (*current_row >> col) & 1;

    // Clear disabled logical bits before APC/RT; acquisition/capture still run.
    if(!ec_cal_enabled(row,col)){
        *current_row &= ~((matrix_row_t)1 << col);
        ec_config.extremum[row][col]=sw_value;
        return current_state;
    }

    // Real Time Noise Floor Calibration
    if (ec_cal_active_valid() && !ec_cal_state && sw_value < (ec_config.noise_floor[row][col] - NOISE_FLOOR_THRESHOLD)) {
        uprintf("Noise Floor Change: %d, %d, %d\n", row, col, sw_value);
        ec_config.noise_floor[row][col]                             = sw_value;
        ec_va_thresholds(row,col);
    }

    if(ec_config.actuation_mode==1){
        bool next=ec_rt_key(row,col,sw_value,current_state);
        if(next)*current_row|=(matrix_row_t)1<<col;else *current_row&=~((matrix_row_t)1<<col);
        return next!=current_state;
    }
    // Normal board-wide APC
    if (ec_config.actuation_mode == 0) {
        if (current_state && sw_value < ec_config.rescaled_mode_0_release_threshold[row][col]) {
            *current_row &= ~(1 << col);
            uprintf("Key released: %d, %d, %d\n", row, col, sw_value);
            return true;
        }
        if ((!current_state) && sw_value > ec_config.rescaled_mode_0_actuation_threshold[row][col]) {
            *current_row |= (1 << col);
            uprintf("Key pressed: %d, %d, %d\n", row, col, sw_value);
            return true;
        }
    }

    return false;
}


#ifndef EC_BOARD_RESCALE
uint16_t rescale(uint16_t x,uint16_t a,uint16_t b,uint16_t c,uint16_t d){return (uint32_t)(x-a)*(d-c)/(b-a)+c;}

#endif

// Hardware adapters call this once per cached 10-bit rising-polarity sample.
// Capture suppresses HID but still feeds sustained-block calibration evidence.
bool ec_process_sample(matrix_row_t *rows,uint8_t row,uint8_t col,uint16_t raw){
 bool changed;
 if(ec_config.bottoming_calibration){changed=rows[row]!=0;rows[row]=0;}
 else changed=ec_update_key(&rows[row],row,col,raw);
 ec_diagnostic_sample(row,col,raw,(rows[row]>>col)&1);
 return changed;
}
