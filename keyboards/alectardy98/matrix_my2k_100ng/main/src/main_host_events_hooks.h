/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
/* Forced into only this board/profile's high-level hal_usbh.c object. Load the
 * original OSAL definitions first. The wrapper performs exactly the original
 * ms-to-tick sleep and adds passive snapshots; shared source remains unchanged. */
#include "hal.h"
#include "main_host_events.h"
#if MY2K_DEV12_PROFILE
#    undef osalThreadSleepMilliseconds
#    define osalThreadSleepMilliseconds(ms) my2k_host_events_sleep_ms((uint32_t)(ms))
#endif
