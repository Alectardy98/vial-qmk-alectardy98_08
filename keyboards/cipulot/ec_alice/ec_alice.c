/* Copyright 2024 Cipulot
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
    eeprom_ec_config.ind1.h       = 0;
    eeprom_ec_config.ind1.s       = 255;
    eeprom_ec_config.ind1.v       = 150;
    eeprom_ec_config.ind1.func    = 0x04;
    eeprom_ec_config.ind1.index   = 0;
    eeprom_ec_config.ind1.enabled = true;

    eeprom_ec_config.ind2.h       = 86;
    eeprom_ec_config.ind2.s       = 255;
    eeprom_ec_config.ind2.v       = 150;
    eeprom_ec_config.ind2.func    = 0x04;
    eeprom_ec_config.ind2.index   = 1;
    eeprom_ec_config.ind2.enabled = true;

    eeprom_ec_config.ind3.h       = 166;
    eeprom_ec_config.ind3.s       = 254;
    eeprom_ec_config.ind3.v       = 150;
    eeprom_ec_config.ind3.func    = 0x04;
    eeprom_ec_config.ind3.index   = 2;
    eeprom_ec_config.ind3.enabled = true;

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
    rgblight_set_effect_range(3, 36);
    // Call the indicator callback to set the indicator color
    indicators_callback();

    keyboard_post_init_user();
}

// This function gets called when caps, num, scroll change
bool led_update_kb(led_t led_state) {
    indicators_callback();
    return true;
}

// This function is called when layers change
__attribute__((weak)) layer_state_t layer_state_set_user(layer_state_t state) {
    indicators_callback();
    return state;
}

bool func_switch(uint8_t func) {
    switch (func) {
        case 0x00: {
            return false;
            break;
        }
        case 0x01: // If indicator is set as caps lock
        {
            if (host_keyboard_led_state().caps_lock) return true;
            break;
        }
        case 0x02: // If indicator is set as num lock
        {
            if (host_keyboard_led_state().num_lock) return true;
            break;
        }
        case 0x03: // If indicator is set as scroll lock
        {
            if (host_keyboard_led_state().scroll_lock) return true;
            break;
        }
        case 0x04: // If indicator is set as layer 0
        case 0x05: // layer 1
        case 0x06: // layer 2
        case 0x07: // layer 3
        case 0x08: // layer 4
        case 0x09: // layer 5
        case 0x0A: // layer 6
        case 0x0B: // layer 7
        {
            if (IS_LAYER_ON((int)(func)-4)) return true;
            break;
        }
        default: {
            return false;
        }
    }
    return false;
}

bool set_indicator(indicator_config indicator) {
    if (!indicator.enabled) return false;
    return func_switch(indicator.func & 0x0F) | func_switch((indicator.func & 0xF0) >> 4);
}

bool indicators_callback(void) {
    // Basic functioning: for each indicator, set_indicator is used to decide if the current indicator should be lit or off.
    indicator_config* current_indicator_p;
    int               index;
    RGB               color;
    for (index = 0; index < 3; index++) {
        current_indicator_p = get_indicator_p(index);
        if (set_indicator(*(current_indicator_p))) {
            /*
               Issue: while the VIA custom GUI returns HSV values, the QMK direct operation funcs are RGB.
               So this line converts the current indicator to RGB. This was not done at the indicator_config_set_value VIA callback function
               because at the indicator_config_get_value the RGB to HSV would be required and this throttles the keyboard
               when the user is adjusting the color on the GUI.
            */
            color = hsv_to_rgb((HSV){current_indicator_p->h, current_indicator_p->s, current_indicator_p->v});
            rgblight_setrgb_at(color.r, color.g, color.b, current_indicator_p->index);
        } else
            rgblight_setrgb_at(RGB_OFF, current_indicator_p->index);
    }
    return true;
}
