/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define MY2K_BRIGHTNESS_MARKER 0x3942524du
#define MY2K_PERFORMANCE_BIT 0x80000000u
void my2k_performance_init(void);
bool my2k_performance_mode_changing(void);
bool my2k_performance_hs_requested(void); /* Effective PHY for this boot. */
bool my2k_performance_high_cpu(void);
bool my2k_performance_usb_start_failed(void);
void my2k_performance_usb_fault(uint32_t bits);
void my2k_performance_usb_enumerated(uint8_t raw_speed);
void my2k_performance_usb_configured(bool configured);
void my2k_performance_usb_reset(void);
uint8_t my2k_performance_usb_speed(void); /*0none,1FS,2HS; cached enumeration. */
void my2k_performance_task(uint32_t now_ms);
bool my2k_performance_reply(uint8_t report[32]);
#if MY2K_PERFORMANCE_HOST
uint32_t my2k_performance_platform_read(void);
void my2k_performance_platform_write(uint32_t word);
bool my2k_performance_platform_durable(void);
bool my2k_performance_platform_pending(void);
bool my2k_performance_platform_busy(void);
bool my2k_performance_platform_idle(void);
bool my2k_performance_platform_export(void);
bool my2k_performance_platform_unlocked(void);
uint32_t my2k_performance_platform_error(void);
uint32_t my2k_performance_platform_now(void);
void my2k_performance_platform_restart(void);
void my2k_performance_platform_usb_fallback(void);
#endif
