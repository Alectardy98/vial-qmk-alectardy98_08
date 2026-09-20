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
#include "quantum.h"

// Definition of SOCD shared instance
socd_cleaner_t socd_opposing_pairs[4];

void eeconfig_init_kb(void) {
    // Default values
    eeprom_ec_config.num.h                          = 0;
    eeprom_ec_config.num.s                          = 0;
    eeprom_ec_config.num.v                          = 60;
    eeprom_ec_config.num.enabled                    = true;
    eeprom_ec_config.caps.h                         = 0;
    eeprom_ec_config.caps.s                         = 0;
    eeprom_ec_config.caps.v                         = 60;
    eeprom_ec_config.caps.enabled                   = true;
    eeprom_ec_config.scroll.h                       = 0;
    eeprom_ec_config.scroll.s                       = 0;
    eeprom_ec_config.scroll.v                       = 60;
    eeprom_ec_config.scroll.enabled                 = true;
    eeprom_ec_config.actuation_mode                 = DEFAULT_ACTUATION_MODE;
    eeprom_ec_config.mode_0_actuation_threshold     = DEFAULT_MODE_0_ACTUATION_LEVEL;
    eeprom_ec_config.mode_0_release_threshold       = DEFAULT_MODE_0_RELEASE_LEVEL;
    eeprom_ec_config.mode_1_initial_deadzone_offset = DEFAULT_MODE_1_INITIAL_DEADZONE_OFFSET;
    eeprom_ec_config.mode_1_actuation_offset        = DEFAULT_MODE_1_ACTUATION_OFFSET;
    eeprom_ec_config.mode_1_release_offset          = DEFAULT_MODE_1_RELEASE_OFFSET;

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            eeprom_ec_config.bottoming_reading[row][col] = DEFAULT_BOTTOMING_READING;
        }
    }

    // Initialize the SOCD cleaner pairs
    const struct {
        uint16_t key1;
        uint16_t key2;
    } socd_pairs[] = {
        {KC_A, KC_D},
        {KC_W, KC_S},
        {KC_Z, KC_X},
        {KC_LEFT, KC_RIGHT},
    };

    for (int i = 0; i < 4; i++) {
        eeprom_ec_config.socd_opposing_pairs[i].keys[0]    = socd_pairs[i].key1;
        eeprom_ec_config.socd_opposing_pairs[i].keys[1]    = socd_pairs[i].key2;
        eeprom_ec_config.socd_opposing_pairs[i].resolution = SOCD_CLEANER_OFF;
        eeprom_ec_config.socd_opposing_pairs[i].held[0]    = false;
        eeprom_ec_config.socd_opposing_pairs[i].held[1]    = false;
    }

    // Write default value to EEPROM now
    eeconfig_update_kb_datablock(&eeprom_ec_config, 0, EECONFIG_KB_DATA_SIZE);

    eeconfig_init_user();
}

// On Keyboard startup
void keyboard_post_init_kb(void) {
    // Read custom menu variables from memory
    eeconfig_read_kb_datablock(&eeprom_ec_config, 0, EECONFIG_KB_DATA_SIZE);

    // Set runtime values to EEPROM values
    ec_config.actuation_mode                 = eeprom_ec_config.actuation_mode;
    ec_config.mode_0_actuation_threshold     = eeprom_ec_config.mode_0_actuation_threshold;
    ec_config.mode_0_release_threshold       = eeprom_ec_config.mode_0_release_threshold;
    ec_config.mode_1_initial_deadzone_offset = eeprom_ec_config.mode_1_initial_deadzone_offset;
    ec_config.mode_1_actuation_offset        = eeprom_ec_config.mode_1_actuation_offset;
    ec_config.mode_1_release_offset          = eeprom_ec_config.mode_1_release_offset;
    ec_config.bottoming_calibration          = false;
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            ec_config.bottoming_calibration_starter[row][col]           = true;
            ec_config.bottoming_reading[row][col]                       = eeprom_ec_config.bottoming_reading[row][col];
            ec_config.rescaled_mode_0_actuation_threshold[row][col]     = rescale(ec_config.mode_0_actuation_threshold, ec_config.noise_floor[row][col], eeprom_ec_config.bottoming_reading[row][col]);
            ec_config.rescaled_mode_0_release_threshold[row][col]       = rescale(ec_config.mode_0_release_threshold, ec_config.noise_floor[row][col], eeprom_ec_config.bottoming_reading[row][col]);
            ec_config.rescaled_mode_1_initial_deadzone_offset[row][col] = rescale(ec_config.mode_1_initial_deadzone_offset, ec_config.noise_floor[row][col], eeprom_ec_config.bottoming_reading[row][col]);
            ec_config.rescaled_mode_1_actuation_offset[row][col]        = rescale(ec_config.mode_1_actuation_offset, ec_config.noise_floor[row][col], eeprom_ec_config.bottoming_reading[row][col]);
            ec_config.rescaled_mode_1_release_offset[row][col]          = rescale(ec_config.mode_1_release_offset, ec_config.noise_floor[row][col], eeprom_ec_config.bottoming_reading[row][col]);
        }
    }

    memcpy(socd_opposing_pairs, eeprom_ec_config.socd_opposing_pairs, sizeof(socd_opposing_pairs));

    // Set the RGB LEDs range that will be used for the effects
    rgblight_set_effect_range(3, 66);
    // Call the indicator callback to set the indicator color
    indicators_callback();

    keyboard_post_init_user();
}

// This function gets called when caps, num, scroll change
bool led_update_kb(led_t led_state) {
    if (led_update_user(led_state)) {
        indicators_callback();
    }
    return true;
}

// This function is called when layers change
__attribute__((weak)) layer_state_t layer_state_set_user(layer_state_t state) {
    indicators_callback();
    return state;
}

// INDICATOR CALLBACK ------------------------------------------------------------------------------
/* LED index to physical position
 *
 * LED2 | LED1 |  LED0
 * -----+------+--------
 * Num  | Caps | Scroll |
 */
bool indicators_callback(void) {
    if ((eeprom_ec_config.num.enabled) && (host_keyboard_led_state().num_lock))
        rgblight_sethsv_at(eeprom_ec_config.num.h, eeprom_ec_config.num.s, eeprom_ec_config.num.v, NUM_INDICATOR_INDEX);
    else
        rgblight_sethsv_at(0, 0, 0, NUM_INDICATOR_INDEX);

    if ((eeprom_ec_config.caps.enabled) && (host_keyboard_led_state().caps_lock))
        rgblight_sethsv_at(eeprom_ec_config.caps.h, eeprom_ec_config.caps.s, eeprom_ec_config.caps.v, CAPS_INDICATOR_INDEX);
    else
        rgblight_sethsv_at(0, 0, 0, CAPS_INDICATOR_INDEX);

    if ((eeprom_ec_config.scroll.enabled) && (host_keyboard_led_state().scroll_lock))
        rgblight_sethsv_at(eeprom_ec_config.scroll.h, eeprom_ec_config.scroll.s, eeprom_ec_config.scroll.v, SCROLL_INDICATOR_INDEX);
    else
        rgblight_sethsv_at(0, 0, 0, SCROLL_INDICATOR_INDEX);

    rgblight_set();
    return true;
}
