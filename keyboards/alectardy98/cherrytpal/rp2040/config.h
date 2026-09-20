/* Copyright 2022 Alectardy98
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

/* Trackpoad */

#define PS2_PIO_USE_PIO1 // Force the usage of PIO1 peripheral, by default the PS2 implementation uses the PIO0 peripheral
#define PS2_MOUSE_USE_REMOTE_MODE // Currently Requred due to bug with PS2 and Button conflicts on the matrix
#define PS2_MOUSE_X_MULTIPLIER 3
#define PS2_MOUSE_Y_MULTIPLIER 3
#define PS2_MOUSE_V_MULTIPLIER 1

// Use right button as the scroll-modifier
#define PS2_MOUSE_SCROLL_BTN_MASK (1 << PS2_MOUSE_BTN_RIGHT)

// If released quicker than 300 ms, send a normal right-click.
// If held longer, suppress the click and enable scroll mode.
#define PS2_MOUSE_SCROLL_BTN_SEND 300

// Optional tuning: higher divisor = slower scroll speed
#define PS2_MOUSE_SCROLL_DIVISOR_H 5
#define PS2_MOUSE_SCROLL_DIVISOR_V 5


/* Audio */
#define AUDIO_PIN GP2
#define AUDIO_PWM_DRIVER PWMD1
#define AUDIO_PWM_CHANNEL RP2040_PWM_CHANNEL_A
#define AUDIO_INIT_DELAY
#define AUDIO_CLICKY
#define AUDIO_VOICES
#define MUSIC_MAP

