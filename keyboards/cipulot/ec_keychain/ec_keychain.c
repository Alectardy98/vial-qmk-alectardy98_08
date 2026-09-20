/* Copyright 2025 Cipulot
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

void eeconfig_init_kb(void) {
    // Default values
    eeprom_ec_config.actuation_mode                 = DEFAULT_ACTUATION_MODE;
    eeprom_ec_config.mode_0_actuation_threshold     = DEFAULT_MODE_0_ACTUATION_LEVEL;
    eeprom_ec_config.mode_0_release_threshold       = DEFAULT_MODE_0_RELEASE_LEVEL;
    eeprom_ec_config.mode_1_initial_deadzone_offset = DEFAULT_MODE_1_INITIAL_DEADZONE_OFFSET;
    eeprom_ec_config.mode_1_actuation_offset        = DEFAULT_MODE_1_ACTUATION_OFFSET;
    eeprom_ec_config.mode_1_release_offset          = DEFAULT_MODE_1_RELEASE_OFFSET;
    eeprom_ec_config.bottoming_reading              = DEFAULT_BOTTOMING_READING;

    // Write default value to EEPROM now
    eeconfig_update_kb_datablock(&eeprom_ec_config, 0, EECONFIG_KB_DATA_SIZE);

    eeconfig_init_user();
}

// On Keyboard startup
void keyboard_post_init_kb(void) {
    // Read custom menu variables from memory
    eeconfig_read_kb_datablock(&eeprom_ec_config, 0, EECONFIG_KB_DATA_SIZE);

    // Set runtime values to EEPROM values
    ec_config.actuation_mode                          = eeprom_ec_config.actuation_mode;
    ec_config.mode_0_actuation_threshold              = eeprom_ec_config.mode_0_actuation_threshold;
    ec_config.mode_0_release_threshold                = eeprom_ec_config.mode_0_release_threshold;
    ec_config.mode_1_initial_deadzone_offset          = eeprom_ec_config.mode_1_initial_deadzone_offset;
    ec_config.mode_1_actuation_offset                 = eeprom_ec_config.mode_1_actuation_offset;
    ec_config.mode_1_release_offset                   = eeprom_ec_config.mode_1_release_offset;
    ec_config.bottoming_calibration                   = false;
    ec_config.bottoming_calibration_starter           = true;
    ec_config.bottoming_reading                       = eeprom_ec_config.bottoming_reading;
    ec_config.rescaled_mode_0_actuation_threshold     = rescale(ec_config.mode_0_actuation_threshold, ec_config.noise_floor, eeprom_ec_config.bottoming_reading);
    ec_config.rescaled_mode_0_release_threshold       = rescale(ec_config.mode_0_release_threshold, ec_config.noise_floor, eeprom_ec_config.bottoming_reading);
    ec_config.rescaled_mode_1_initial_deadzone_offset = rescale(ec_config.mode_1_initial_deadzone_offset, ec_config.noise_floor, eeprom_ec_config.bottoming_reading);
    ec_config.rescaled_mode_1_actuation_offset        = rescale(ec_config.mode_1_actuation_offset, ec_config.noise_floor, eeprom_ec_config.bottoming_reading);
    ec_config.rescaled_mode_1_release_offset          = rescale(ec_config.mode_1_release_offset, ec_config.noise_floor, eeprom_ec_config.bottoming_reading);

    keyboard_post_init_user();
}
