/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
/* Board-local adapter for the pinned ChibiOS OTGv1 driver's FS=5/HS=9
 * constants. Original Geehy loader 0x08001e4c uses 6 at HCLK=168 MHz,
 * speed argument 2; other speed arguments select 9. This is distinct from
 * the application's alternative PHY setup. No silicon validation implied. */
static inline uint32_t my2k_geehy_usb_turnaround(uint32_t chibios_cycles) {
    return chibios_cycles == 5u ? 6u : chibios_cycles;
}
