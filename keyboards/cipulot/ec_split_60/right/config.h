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

#pragma once

#define MATRIX_ROW_PINS {B6, B7, B8, A2, A1, B6, B7, B8, A2, A1}

#define AMUX_COUNT 1
#define AMUX_MAX_COLS_COUNT 8

#define AMUX_EN_PINS {B5}

#define AMUX_SEL_PINS {B4, B3, A15}

#define AMUX_COL_CHANNELS_SIZES {8}

#define AMUX_0_COL_CHANNELS {5, 7, 6, 4, 3, 0, 2, 1}

#define AMUX_COL_CHANNELS AMUX_0_COL_CHANNELS

#define DISCHARGE_PIN A6
#define ANALOG_PORT A7
