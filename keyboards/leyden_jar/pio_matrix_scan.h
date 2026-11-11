/* Copyright 2022 Eric Becourt (Rico https://mymakercorner.com)
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

void pio_matrix_scan_init(bool enable_extra_cols);
void pio_raw_scan(void);
const uint8_t* pio_get_scan_vals(void);
#ifdef LEYDEN_JAR_GPIO_COLUMNS
bool pio_gpio_columns_ready(void);
void pio_gpio_columns_status(uint8_t payload[28]);
#endif
