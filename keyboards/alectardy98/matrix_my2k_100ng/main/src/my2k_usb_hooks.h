/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
/* Force-included for QMK's usb_main.c ONLY. Preserve its complete HID service,
 * intercepting the configuration at the documented HAL startup boundary. */
#include "hal.h"
#include "my2k_usb_bus_hooks.h"
msg_t my2k_usb_start(USBDriver *driver, const USBConfig *configuration);
#define usbStart my2k_usb_start
#if MY2K_DEV4_PROFILE
#    include "usb_driver.h"
#    include "main_perf.h"
#    if MY2K_DEV16_PROFILE
#        include "main_input_performance.h"
#        include "usb_descriptor.h"
#        include "report.h"
#    endif
/* Measures host-queue submission only, not successful transmission or polling. */
static inline bool my2k_perf_usb_send(usb_endpoint_in_t *endpoint, const uint8_t *data, size_t size,
                                      sysinterval_t timeout, bool buffered) {
    uint32_t start  = my2k_perf_clock();
    bool     result = usb_endpoint_in_send(endpoint, data, size, timeout, buffered);
#    if MY2K_DEV16_PROFILE
    bool keyboard = endpoint->config.ep == KEYBOARD_IN_EPNUM;
#        ifdef NKRO_ENABLE
    if (endpoint->config.ep == SHARED_IN_EPNUM && size && data[0] == REPORT_ID_NKRO)
        keyboard = true;
#        endif
    if (keyboard)
        my2k_input_report_queued(result, start, my2k_perf_clock());
#    endif
    my2k_perf_record(MY2K_PERF_USB, start);
    return result;
}
#    define usb_endpoint_in_send my2k_perf_usb_send
#endif
