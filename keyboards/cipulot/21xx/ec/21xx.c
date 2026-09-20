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

#include "quantum.h"

void matrix_init_kb(void) {
    // Init the LED pins as outputs
    gpio_set_pin_output(LED_CAPS_LOCK_PIN);
    gpio_set_pin_output(LED_NUM_LOCK_PIN);
    gpio_set_pin_output(LED_LAYER_INDICATOR_1_PIN);
    gpio_set_pin_output(LED_LAYER_INDICATOR_2_PIN);
    gpio_set_pin_output(LED_LAYER_INDICATOR_3_PIN);
    gpio_set_pin_output(LED_KEYPRESS_INDICATOR_PIN);
    gpio_set_pin_output(LED_HAPTIC_ENABLE_STATUS_PIN);

    gpio_write_pin_high(LED_CAPS_LOCK_PIN);
    gpio_write_pin_high(LED_NUM_LOCK_PIN);
    gpio_write_pin_high(LED_LAYER_INDICATOR_1_PIN);
    gpio_write_pin_high(LED_LAYER_INDICATOR_2_PIN);
    gpio_write_pin_high(LED_LAYER_INDICATOR_3_PIN);
    gpio_write_pin_high(LED_KEYPRESS_INDICATOR_PIN);
    gpio_write_pin_high(LED_HAPTIC_ENABLE_STATUS_PIN);

    matrix_init_user();
}
