/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Board-local, independently expressed behavior recovered from stock
 * 0x08034A40 and corroborated by the exact-part Geehy SDK v1.5.3.
 * These byte-width shadow registers belong to embedded HS2, not ULPI.
 * No flash or internal AUX-host register is accessed. */
#define MY2K_HS2_FAULT_AHB_IDLE (1u << 0)
#define MY2K_HS2_FAULT_CORE_RESET (1u << 1)
#define MY2K_HS2_FAULT_FS_FALLBACK (1u << 2)
#define MY2K_HS2_FAULT_START_FAILED (1u << 3)
#ifndef MY2K_HS2_SHADOW_WINDOW
#    define MY2K_HS2_SHADOW_WINDOW ((volatile uint8_t *)(uintptr_t)0x40040200u)
#endif

/* Finite read budget, not a wall-clock timeout: startup runs with the OS lock.
 * It must not wait on SysTick or a sleeping worker. */
#ifndef MY2K_HS2_WAIT_READ_BUDGET
#    define MY2K_HS2_WAIT_READ_BUDGET 100000u
#endif
#ifndef MY2K_HS2_READ32
#    define MY2K_HS2_READ32(reg) (*(reg))
#endif
#ifndef MY2K_HS2_WRITE8
#    define MY2K_HS2_WRITE8(window, offset, value) ((window)[offset] = (value))
#endif

static inline void my2k_hs2_phy_prepare(volatile uint8_t *window) {
    MY2K_HS2_WRITE8(window, 0x00, (uint8_t)(window[0x00] | 1u));
    MY2K_HS2_WRITE8(window, 0x04, (uint8_t)(window[0x04] | 1u));
    MY2K_HS2_WRITE8(window, 0x10, (uint8_t)(window[0x10] | 1u));
    MY2K_HS2_WRITE8(window, 0x24, (uint8_t)((window[0x24] & 0xf8u) | 5u));
}

/* Both an initial Standard start and a controller-only recovery explicitly
 * select the ordinary FS alternative. Do not depend on RCC reset reaching
 * the alternative-core shadow selector. Physical fallback remains unproven. */
static inline void my2k_hs2_phy_deselect(volatile uint8_t *window) {
    MY2K_HS2_WRITE8(window, 0x00, (uint8_t)(window[0x00] & (uint8_t)~1u));
}

static inline bool my2k_hs2_wait_bits(volatile uint32_t *reg, uint32_t mask, uint32_t value) {
    for (uint32_t reads = 0; reads < MY2K_HS2_WAIT_READ_BUDGET; ++reads) {
        if ((MY2K_HS2_READ32(reg) & mask) == value) {
            return true;
        }
    }
    return false;
}

/* The local LLD owns this RAM flag. Connect helpers must keep the bus soft-
 * disconnected after the final bounded FS recovery attempt fails. */
bool my2k_hs2_usb_start_failed(void);
