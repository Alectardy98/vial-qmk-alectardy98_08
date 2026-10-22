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

#include "ec_switch_matrix.h"
#include "keyboard.h"
#include "calibration.h"
#include <string.h>

#ifdef SPLIT_KEYBOARD
#    include "transactions.h"
#endif

void eeconfig_init_kb(void) {
    // Missing EC data is initialized in RAM only. Explicit calibration Save owns writes.
    ec_cal_empty();

    eeconfig_init_user();
}

void ec_load_runtime_record(void) {

    ec_config.bottoming_calibration=false;
    for(uint8_t row=0;row<MATRIX_ROWS;++row)for(uint8_t col=0;col<MATRIX_COLS;++col){
        ec_config.bottoming_calibration_starter[row][col]=true;
        ec_config.bottoming_reading[row][col]=ec_cal_bottom(row,col);
    }
    ec_cal_runtime();

}

void keyboard_post_init_kb(void) {
    memset(&eeprom_ec_config,0,sizeof eeprom_ec_config);
    eeconfig_read_kb_datablock(&eeprom_ec_config, 0, EECONFIG_KB_DATA_SIZE);
    ec_va_load();
    ec_load_runtime_record();
    keyboard_post_init_user();
}
