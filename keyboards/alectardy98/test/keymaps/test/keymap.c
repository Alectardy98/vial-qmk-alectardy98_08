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

#include QMK_KEYBOARD_H

void max7219_init(void);
void max7219_send(uint8_t address, uint8_t data);


#include "timer.h"

static uint32_t last_update = 0;
static uint8_t segment_index = 0;

void matrix_scan_user(void) {
    if (timer_elapsed32(last_update) < 500) {
        return;  // Don't update yet
    }

    uint8_t segment_mask = (1 << segment_index);

    // Light same segment on all digits
    for (uint8_t digit = 1; digit <= 8; digit++) {
        max7219_send(digit, segment_mask);
    }

    // Advance to next segment
    segment_index = (segment_index + 1) % 8;
    last_update = timer_read32();
}

enum layer_names {  //Define Layers
    _FN
};


const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_FN] = LAYOUT(
        _______
    )
};


