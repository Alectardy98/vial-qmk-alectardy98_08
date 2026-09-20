/*
Copyright 2020-2023 Purdea Andrei

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

#undef MATRIX_ROWS
#undef MATRIX_COLS
#define MATRIX_ROWS 9      // 8 capsense + 1 extra-direct row
#define MATRIX_COLS 13     // keep your board’s 13 columns

// — Extra direct row (GPIO keys) —
#define MATRIX_EXTRA_DIRECT_ROWS 1
#define MATRIX_EXTRA_DIRECT_COLS 13
#define MATRIX_EXTRA_DIRECT_PINS_NEED_INTERNAL_PULLUP 1
#define MATRIX_EXTRA_DIRECT_PINS_ACTIVE_LOW 1

// Put the extra key at row 8, column 12 -> B3 (change B3 if you pick a different pin)
#undef MATRIX_EXTRA_DIRECT_PINS
#define MATRIX_EXTRA_DIRECT_PINS { \
  { NO_PIN, NO_PIN, NO_PIN, NO_PIN, NO_PIN, NO_PIN, NO_PIN, NO_PIN, \
    NO_PIN, NO_PIN, NO_PIN, NO_PIN, B3 } \
}


#define DEF_SERIAL_NUMBER "purdea.ro:qmk_xwhatsit"

#ifndef SERIAL_NUMBER
#define SERIAL_NUMBER DEF_SERIAL_NUMBER
#endif

#define CONTROLLER_IS_UNIVERSAL_MODEL_F

// By default we set up for support of xwhatsit's solenoid driver board.
// Comment out HAPTIC_ENABLE_PIN if you don't have an enable pin:
#define HAPTIC_ENABLE_PIN D3
// Change this if you are using a different pin for the solenoid:
#define SOLENOID_PIN D2
// If you are not using a solenoid then comment out the above, and also in rules.mk, remove HAPTIC_ENABLE
// We disable haptic feedbeck during USB low power conditions:
#define HAPTIC_OFF_IN_LOW_POWER 1
// You can also tune the following for your solenoid:
#define SOLENOID_DEFAULT_DWELL 5
#define SOLENOID_MIN_DWELL 4
//#define SOLENOID_MAX_DWELL 100
#define NO_HAPTIC_MOD





#define DEBUG_MATRIX_SCAN_RATE
