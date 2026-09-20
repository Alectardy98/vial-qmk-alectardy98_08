/*
Copyright 2011 Jun Wako <wakojun@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * scan matrix
 */
#include <stdint.h>
#include <stdbool.h>
#include <util/delay.h>
#include "print.h"
#include "debug.h"
#include "util.h"
#include "timer.h"
#include "matrix.h"
#include "avr/timer_avr.h"
#include <avr/wdt.h>
#include "suspend.h"
#include "news.h"
#include "lufa.h"


void matrix_init_custom(void)
{    debug_enable = true;
    debug_keyboard = true;
    serial_init();
}


bool matrix_scan_custom(matrix_row_t current_matrix[])
{
    uint8_t   keycode  = serial_recv();
    uint8_t   matrix_changed = 0;
    uint8_t   row = 0, col = 0;
    matrix_row_t col_pos = 0;        // ← widen the mask to match your MATRIX_COLS

    // transpose decode (you already have this):
    col = (keycode & 0x78) >> 3;
    row =  keycode & 0x7;

    // now build the mask as a matrix_row_t
    col_pos = ((matrix_row_t)1 << col);

    if (!(keycode & 0x7F)) {
        return matrix_changed;
    }

    if (!(keycode & 0x80)) {
        // key press
        matrix_changed = (current_matrix[row] ^ col_pos) >> col;
        current_matrix[row] |= col_pos;
    } else {
        // key release
        matrix_changed = ((current_matrix[row] & col_pos) ^ 0) >> col;
        current_matrix[row] &= ~col_pos;
    }

    return 1;
}
