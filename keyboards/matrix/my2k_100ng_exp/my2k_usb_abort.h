/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "hal.h"
typedef struct {
    uint8_t  stage;
    uint32_t started;
    bool     failed;
} my2k_usb_abort;
/* Task context under system lock; one bounded register step per call. No USB
 * core/MCU reset. Uses only the dedicated MSC endpoint; does not flush shared RX. */
void my2k_usb_abort_begin(my2k_usb_abort *, uint32_t now);
bool my2k_usb_abort_pollI(my2k_usb_abort *, USBDriver *, usbep_t, uint32_t now);
