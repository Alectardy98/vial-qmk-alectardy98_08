/* Copyright 2021 @ KindleStar(https://www.KindleStar.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

/* key matrix size */
#define MATRIX_ROWS 6
#define MATRIX_COLS 16

/* Set 0 if debouncing isn't needed */
#define DEBOUNCE 5


// Data-in pin for WS2812
#define WS2812_DI_PIN B9
#define WS2812_LED_COUNT 87


#define BACKLIGHT_PIN B11
#define BACKLIGHT_PWM_DRIVER    PWMD2
#define BACKLIGHT_PWM_CHANNEL   4
#define BACKLIGHT_PAL_MODE      2

#ifdef RGB_MATRIX_ENABLE
#define START_BREATH_IDX 14

#define RGB_MATRIX_STARTUP_HUE 170
#define RGB_MATRIX_STARTUP_VAL 255
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 160

#define RGB_MATRIX_FRAMEBUFFER_EFFECTS
#define RGB_MATRIX_KEYPRESSES
#define RGB_MATRIX_LED_FLUSH_LIMIT 26
#define RGB_MATRIX_SLEEP  // turn off effects when suspended
#define RGB_MATRIX_STARTUP_MODE RGB_MATRIX_RAINBOW_BEACON

#define ENABLE_RGB_MATRIX_RAINBOW_BEACON
#define ENABLE_RGB_MATRIX_RAINBOW_PINWHEELS
#define ENABLE_RGB_MATRIX_CYCLE_SPIRAL
#define ENABLE_RGB_MATRIX_RAINBOW_MOVING_CHEVRON
#define ENABLE_RGB_MATRIX_BAND_SPIRAL_VAL
#define ENABLE_RGB_MATRIX_SOLID_MULTISPLASH
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_MULTICROSS

#endif

#define KEYSTAR_LED_RED_ON() {}
#define KEYSTAR_LED_GREEN_ON() {}
#define KEYSTAR_LED_BLUE_ON() {}

#define KEYSTAR_LED_RED_OFF() {}
#define KEYSTAR_LED_GREEN_OFF() {}
#define KEYSTAR_LED_BLUE_OFF() {}

