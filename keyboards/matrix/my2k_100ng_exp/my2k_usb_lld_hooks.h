/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
/* Force-included only in this board's hal_usb_lld.c. hal.h defines the original
 * RCC reset macro before the wrapper is compiled. No upstream HAL edits. */
#include "hal.h"
#include "my2k_geehy_usb_shadow.h"
#include "my2k_geehy_usb_timing.h"
#if MY2K_DEV16_HS2
#include "main_performance_mode.h"
#include "my2k_hs2_usb.h"
#if MY2K_RECOVERED_USB_PHY_ENABLE
#error "Dev16 runtime HS2 preparation replaces the conservative shadow-only gate"
#endif
#endif
/* This target uses only OTG2 with the full-speed PHY and fixed 168 MHz HCLK.
 * Adjust the formatter in this TU so both startup and ENUMDNE use the stock
 * loader's FS turnaround. Keep the upstream HAL and other boards untouched. */
#if STM32_HCLK != 168000000 || STM32_USB_USE_OTG1 || defined(BOARD_OTG2_USES_ULPI)
#error "Review MY2K recovered USB timing for the changed clock/PHY configuration"
#endif
#undef GUSBCFG_TRDT
#if MY2K_DEV16_HS2
/* Stock HS2 uses TRDT9 for its HS PHY, including FS fallback negotiated on
 * that PHY. Standard mode keeps the physically established FS TRDT6 path. */
#define GUSBCFG_TRDT(n) ((my2k_performance_hs_requested() ? 9u : 6u) << 10)
#else
#define GUSBCFG_TRDT(n) (my2k_geehy_usb_turnaround(n) << 10)
#endif
#if MY2K_RECOVERED_USB_PHY_ENABLE
static inline void my2k_otg2_reset_and_prepare(void) {
    /* usb_lld_start enables OTG_HS before calling this. Preparation must follow
     * the RCC reset, and precede the normal DWC2 register initialization. */
    rccResetOTG_HS();
    my2k_geehy_usb_shadow_prepare((volatile uint8_t *)(uintptr_t)0x40040200u);
}
#undef rccResetOTG_HS
#define rccResetOTG_HS() my2k_otg2_reset_and_prepare()
#endif
