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
/* Rewritten EC42 console view derived from the original via_ec.c interface.
 * Framework modifications; attribution updated 2026-10-21. */
#include "ec_switch_matrix.h"
#include "calibration.h"
#include "quantum.h"
void ec_show_calibration_data(void){
    uprintf("EC calibration method=%u state=%u valid=%u\n",ec_cal_method(),ec_cal_state,ec_cal_active_valid());
    for(uint8_t r=0;r<MATRIX_ROWS;++r)for(uint8_t c=0;c<MATRIX_COLS;++c){
        if(is_unused_position(r,c))continue;
        uprintf("%u,%u baseline=%u press=%u release=%u\n",r,c,ec_config.noise_floor[r][c],ec_config.rescaled_mode_0_actuation_threshold[r][c],ec_config.rescaled_mode_0_release_threshold[r][c]);
    }
}
