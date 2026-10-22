/* Copyright 2023 Cipulot
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Read-only legacy VIA inspection; calibration writes use the v2.3 transaction API.
 */
#include "ec_switch_matrix.h"
#include "calibration.h"
#include "quantum.h"
void via_config_get_value(uint8_t *d){
    uint16_t value=0;switch(d[0]){
        case 1:d[1]=eeprom_ec_config.actuation_mode;return;
        case 2:value=eeprom_ec_config.mode_0_actuation_threshold;break;
        case 3:value=eeprom_ec_config.mode_0_release_threshold;break;
        case 5:value=eeprom_ec_config.mode_1_initial_deadzone_offset;break;
        case 6:d[1]=eeprom_ec_config.mode_1_actuation_offset;return;
        case 7:d[1]=eeprom_ec_config.mode_1_release_offset;return;
        default:return;
    }d[1]=value>>8;d[2]=value;
}
void via_custom_value_command_kb(uint8_t *d,uint8_t n){
    if(n==32&&d[0]==8&&d[1]==0){via_config_get_value(d+2);return;}d[0]=0xff;
}
void ec_show_calibration_data(void){
    uprintf("EC calibration method=%u state=%u valid=%u\n",ec_cal_method(),ec_cal_state,ec_cal_active_valid());
    for(uint8_t r=0;r<MATRIX_ROWS;++r)for(uint8_t c=0;c<MATRIX_COLS;++c){
        if(is_unused_position(r,c))continue;
        uprintf("%u,%u baseline=%u press=%u release=%u\n",r,c,ec_config.noise_floor[r][c],ec_config.rescaled_mode_0_actuation_threshold[r][c],ec_config.rescaled_mode_0_release_threshold[r][c]);
    }
}
