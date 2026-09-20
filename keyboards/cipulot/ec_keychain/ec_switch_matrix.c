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
#include "analog.h"
#include "atomic_util.h"
#include "math.h"
#include "print.h"
#include "wait.h"

#if defined(__AVR__)
#    error "AVR platforms not supported due to a variety of reasons. Among them there are limited memory, limited number of pins and ADC not being able to give satisfactory results."
#endif

#define OPEN_DRAIN_SUPPORT defined(PAL_MODE_OUTPUT_OPENDRAIN)

eeprom_ec_config_t eeprom_ec_config;
ec_config_t        ec_config;

static uint16_t sw_value;

static adc_mux adcMux;

// Initialize the row pins
void init_row(void) {
    // Set all row pins as output and low
    gpio_set_pin_output(MATRIX_ROW_PIN);
    gpio_write_pin_low(MATRIX_ROW_PIN);
}

// Discharge the peak hold capacitor
void discharge_capacitor(void) {
#ifdef OPEN_DRAIN_SUPPORT
    gpio_write_pin_low(DISCHARGE_PIN);
#else
    gpio_write_pin_low(DISCHARGE_PIN);
    gpio_set_pin_output(DISCHARGE_PIN);
#endif
}

// Charge the peak hold capacitor
void charge_capacitor(void) {
#ifdef OPEN_DRAIN_SUPPORT
    gpio_write_pin_high(DISCHARGE_PIN);
#else
    gpio_set_pin_input(DISCHARGE_PIN);
#endif
    gpio_write_pin_high(MATRIX_ROW_PIN);
}

// Initialize the peripherals pins
int ec_init(void) {
    // Initialize ADC
    palSetLineMode(ANALOG_PORT, PAL_MODE_INPUT_ANALOG);
    adcMux = pinToMux(ANALOG_PORT);

    // Dummy call to make sure that adcStart() has been called in the appropriate state
    adc_read(adcMux);

    // Initialize discharge pin as discharge mode
    gpio_write_pin_low(DISCHARGE_PIN);
#ifdef OPEN_DRAIN_SUPPORT
    gpio_set_pin_output_open_drain(DISCHARGE_PIN);
#else
    gpio_set_pin_output(DISCHARGE_PIN);
#endif

    // Initialize drive lines
    init_row();

    return 0;
}

// Get the noise floor
void ec_noise_floor(void) {
    // Initialize the noise floor
    ec_config.noise_floor = 0;

    // Sample the noise floor
    for (uint8_t i = 0; i < DEFAULT_NOISE_FLOOR_SAMPLING_COUNT; i++) {
        ec_config.noise_floor += ec_readkey_raw();
        wait_ms(5);
    }

    // Average the noise floor
    ec_config.noise_floor /= DEFAULT_NOISE_FLOOR_SAMPLING_COUNT;
}

// Scan matrix and check for ghost key patterns
bool ec_matrix_scan(matrix_row_t current_matrix[]) {
    bool updated = false;

    sw_value = ec_readkey_raw();

    if (ec_config.bottoming_calibration) {
        if (ec_config.bottoming_calibration_starter) {
            ec_config.bottoming_reading             = sw_value;
            ec_config.bottoming_calibration_starter = false;
        } else if (sw_value > ec_config.bottoming_reading) {
            ec_config.bottoming_reading = sw_value;
        }
    } else {
        updated |= ec_update_key(&current_matrix[0], sw_value);
    }

    return ec_config.bottoming_calibration ? false : updated;
}

// Read the capacitive sensor value
uint16_t ec_readkey_raw(void) {
    uint16_t sw_value = 0;

    // Set the row pin to low state to avoid ghosting
    gpio_write_pin_low(MATRIX_ROW_PIN);

    ATOMIC_BLOCK_FORCEON {
        // Set the row pin to high state and have capacitor charge
        charge_capacitor();
        // Read the ADC value
        sw_value = adc_read(adcMux);
    }
    // Discharge peak hold capacitor
    discharge_capacitor();
    // Waiting for the ghost capacitor to discharge fully
    wait_us(DISCHARGE_TIME);

    return sw_value;
}

// Update press/release state of key
bool ec_update_key(matrix_row_t* current_row, uint16_t sw_value) {
    bool current_state = (*current_row >> 0) & 1;

    // Real Time Noise Floor Calibration
    if (sw_value < (ec_config.noise_floor - NOISE_FLOOR_THRESHOLD)) {
        uprintf("Noise Floor Change: %d\n", sw_value);
        ec_config.noise_floor                             = sw_value;
        ec_config.rescaled_mode_0_actuation_threshold     = rescale(ec_config.mode_0_actuation_threshold, ec_config.noise_floor, eeprom_ec_config.bottoming_reading);
        ec_config.rescaled_mode_0_release_threshold       = rescale(ec_config.mode_0_release_threshold, ec_config.noise_floor, eeprom_ec_config.bottoming_reading);
        ec_config.rescaled_mode_1_initial_deadzone_offset = rescale(ec_config.mode_1_initial_deadzone_offset, ec_config.noise_floor, eeprom_ec_config.bottoming_reading);
    }

    // Normal board-wide APC
    if (ec_config.actuation_mode == 0) {
        if (current_state && sw_value < ec_config.rescaled_mode_0_release_threshold) {
            *current_row &= ~(1 << 0);
            uprintf("Key released: %d\n", sw_value);
            return true;
        }
        if ((!current_state) && sw_value > ec_config.rescaled_mode_0_actuation_threshold) {
            *current_row |= (1 << 0);
            uprintf("Key pressed: %d\n", sw_value);
            return true;
        }
    }
    // Rapid Trigger
    else if (ec_config.actuation_mode == 1) {
        // Is key in active zone?
        if (sw_value > ec_config.rescaled_mode_1_initial_deadzone_offset) {
            // Is key pressed while in active zone?
            if (current_state) {
                // Is the key still moving down?
                if (sw_value > ec_config.extremum) {
                    ec_config.extremum = sw_value;
                    uprintf("Key pressed: %d\n", sw_value);
                }
                // Has key moved up enough to be released?
                else if (sw_value < ec_config.extremum - ec_config.rescaled_mode_1_release_offset) {
                    ec_config.extremum = sw_value;
                    *current_row &= ~(1 << 0);
                    uprintf("Key released: %d\n", sw_value);
                    return true;
                }
            }
            // Key is not pressed while in active zone
            else {
                // Is the key still moving up?
                if (sw_value < ec_config.extremum) {
                    ec_config.extremum = sw_value;
                }
                // Has key moved down enough to be pressed?
                else if (sw_value > ec_config.extremum + ec_config.rescaled_mode_1_actuation_offset) {
                    ec_config.extremum = sw_value;
                    *current_row |= (1 << 0);
                    uprintf("Key pressed: %d\n", sw_value);
                    return true;
                }
            }
        }
        // Key is not in active zone
        else {
            // Check to avoid key being stuck in pressed state near the active zone threshold
            if (sw_value < ec_config.extremum) {
                ec_config.extremum = sw_value;
                *current_row &= ~(1 << 0);
                return true;
            }
        }
    }
    return false;
}

// Print the matrix values
void ec_print_matrix(void) {
    uprintf("%4d\n", sw_value);
    print("\n");
}

// Rescale the value to a different range
uint16_t rescale(uint16_t x, uint16_t out_min, uint16_t out_max) {
    return x  * (out_max - out_min) / 1023 + out_min;
}
