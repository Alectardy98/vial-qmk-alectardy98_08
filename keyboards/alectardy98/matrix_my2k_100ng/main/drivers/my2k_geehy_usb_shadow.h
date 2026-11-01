/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
/* Independently expressed register operations observed in original main
 * 0x08034a7c..0x08034aa0 (UF2 and full dump byte-identical). Byte accesses
 * matter: do not widen these to the STM32/DWC2 word register convention.
 * Caller supplies the shadow-register window at 0x40040200, after clocks and
 * GPIO, before the ordinary device-core initializer. This is only PHY prep,
 * not complete USB initialization. Offset 0x24 semantics/errata remain unknown;
 * the board activation gate MUST stay off pending hardware-contract review.
 */
static inline void my2k_geehy_usb_shadow_prepare(volatile uint8_t *window) {
    window[0x00] = (uint8_t)(window[0x00] | 1u);
    window[0x04] = (uint8_t)(window[0x04] | 1u);
    window[0x10] = (uint8_t)(window[0x10] | 1u);
    window[0x24] = (uint8_t)((window[0x24] & 0xf8u) | 5u);
}
