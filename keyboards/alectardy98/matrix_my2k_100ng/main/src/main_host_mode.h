/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
/* Cache-only diagnostics. No register reads occur from the HID handler. */
void my2k_host_mode_status(uint8_t report[32]);

#if MY2K_DEV9_PROFILE
/* Called once by the board's internal-host adapter after successful startup. */
void my2k_dev9_host_discovery(void);
/* Cached startup registers plus current RAM root-port state, never MMIO. */
void my2k_host_port_status(uint8_t report[32]);
#endif
