/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "i2c_master.h"
#if MY2K_DEV2_PROFILE
/* Included QMK families call i2c_init themselves. Keep every such call on the
 * same transport; generic init would remux PB6/PB7 back to hardware AF4. */
void my2k_rgb_bus_init(void);
#    define i2c_init my2k_rgb_bus_init
#endif
/* QMK driver translation units only: latch first failure, skip subsequent writes. */
i2c_status_t my2k_rgb_i2c_write_register(uint8_t address, uint8_t reg, const uint8_t *data, uint16_t size,
                                         uint16_t timeout);
