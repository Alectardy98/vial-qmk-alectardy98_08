/*
Copyright 2019 Fate

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

#pragma once



/* Use 1000hz polling */
#define USB_POLLING_INTERVAL_MS 1

/* Debounce reduces chatter (unintended double-presses) - set 0 if debouncing is not needed */
#define DEBOUNCE 15

#define FORCE_NKRO


#define SOLENOID_PIN D5
#define HAPTIC_ENABLE_PIN D3
#define SOLENOID_DEFAULT_DWELL 4
#define SOLENOID_MIN_DWELL 4
#define SOLENOID_MAX_DWELL  40
#define HAPTIC_OFF_IN_LOW_POWER 1
#define NO_HAPTIC_MOD

