/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "hal.h"
#if MY2K_DEV16_PROFILE
#include "main_performance_mode.h"
#endif
#if MY2K_GEEHY_FS_USB_ENABLE
#if !defined(BOARD_OTG_NOVBUSSENS) || STM32_USB_USE_OTG1 || defined(BOARD_OTG2_USES_ULPI)
#error "MY2K FS USB adaptation requires OTG2 internal FS PHY and no VBUS sensing"
#endif
/* The pinned F405 driver connects by toggling GCCFG.VBUSBSEN. That operation
 * is inconsistent with the original loader's NOVBUSSENS configuration.
 * Original instructions 080028b8/080028d4 instead clear/set DCTL[1]. Keep
 * the override local to this board and the QMK bus-control translation units.
 * This does not select the original application's alternate PHY. */
#undef usb_lld_connect_bus
#undef usb_lld_disconnect_bus
static inline void my2k_usb_bus_connect(USBDriver *usbp) {
    /* The same original routines ungate PHY/AHB clocks before DCTL access. */
#if MY2K_DEV16_PROFILE
    if(my2k_performance_usb_start_failed()){usbp->otg->DCTL|=DCTL_SDIS;return;}
#endif
    usbp->otg->PCGCCTL &= ~3u;
    usbp->otg->DCTL &= ~DCTL_SDIS;
}
static inline void my2k_usb_bus_disconnect(USBDriver *usbp) {
    usbp->otg->PCGCCTL &= ~3u;
    usbp->otg->DCTL |= DCTL_SDIS;
}
#define usb_lld_connect_bus(usbp) my2k_usb_bus_connect(usbp)
#define usb_lld_disconnect_bus(usbp) my2k_usb_bus_disconnect(usbp)
#endif
