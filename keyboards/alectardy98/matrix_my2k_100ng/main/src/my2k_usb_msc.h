/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
/* Task context only; never executes USB-host reads in USB ISR/matrix scan. */
void my2k_usb_msc_task(uint32_t now_ms);

#if MY2K_DEV14_PROFILE
bool my2k_usb_msc_diag(uint8_t page, uint8_t index, uint8_t report[32]);
#endif

#if MY2K_RELEASE1_PROFILE
bool my2k_usb_msc_exit_safe(void);
#endif

/* Includes receive/delivery windows not yet represented by the host queue. */
bool my2k_usb_animation_transfer_busy(void);

bool my2k_usb_animation_bulk_available(void);

void my2k_usb_animation_service(void);
