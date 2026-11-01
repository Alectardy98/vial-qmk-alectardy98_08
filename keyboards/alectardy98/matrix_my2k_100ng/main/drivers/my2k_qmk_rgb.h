/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_QMK_RGB_H
#define MY2K_QMK_RGB_H
#include <stdbool.h>
#include "my2k_rgb.h"
typedef struct { uint32_t writes, acknowledged, frames; uint16_t nonzero_channels; int8_t last_status; uint8_t last_address,last_register,startup_test; } my2k_rgb_diagnostics;
my2k_rgb_diagnostics my2k_qmk_rgb_diagnostics(void);
typedef struct {
 uint32_t writes, acknowledged, nacks, timeouts, pwm_updates;
 uint16_t nonzero_channels;
 uint8_t last_register,initialized,pre[4],post[4],sample[3];
 int8_t last_status,read_status;
} my2k_rgb_controller_diagnostics;
my2k_rgb_controller_diagnostics my2k_qmk_rgb_controller(unsigned index);
#if MY2K_DEV3_PROFILE
#define MY2K_RGB_BOUNDARY __attribute__((noipa,externally_visible))
#else
#define MY2K_RGB_BOUNDARY
#endif
MY2K_RGB_BOUNDARY bool my2k_qmk_rgb_test_start(uint8_t group_mask,uint8_t color,uint16_t duration_ms,uint32_t now);
void my2k_qmk_rgb_test_cancel(void);
uint8_t my2k_qmk_rgb_test_group(void);
/* Concrete I2C1/QMK LED-driver adapter, explicit activation only.
 * Conservative profiles retain GCC0; development profiles use GCC8/PWM<=16.
 * Board-local wrappers latch I2C failures and invalidate initialization.
 * A successful flush reports acknowledged writes, not measured physical light. */
bool my2k_qmk_rgb_init(bool hardware_authorized);
bool my2k_qmk_rgb_flush(const my2k_rgb_state *state);
/* Transient visual blank. Does not alter configuration, current or shutdown. */
void my2k_qmk_rgb_visual_blank(bool blank);
/* Retry initialization with 100ms..2s bounded backoff; call only under the
 * existing physical-output authorization gate. Never enables current itself. */
MY2K_RGB_BOUNDARY bool my2k_qmk_rgb_service(const my2k_rgb_state *, uint32_t now_ms, bool authorized);
uint8_t my2k_qmk_rgb_present(void);
uint8_t my2k_qmk_rgb_families(void);
uint32_t my2k_qmk_rgb_errors(void);
#endif
