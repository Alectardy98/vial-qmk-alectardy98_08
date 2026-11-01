/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
/* A separate application asset mode, never ASTRO firmware-update mode. */
void my2k_asset_mode_init(void);
bool my2k_asset_export_active(void);
bool my2k_asset_transition_pending(void);
bool my2k_asset_file_access_allowed(void);
/* Read-only F24 asset mode. F24 schedules a bounded application-mode switch. */
bool my2k_asset_mode_request(bool pressed,uint32_t now);
#if MY2K_DEV15_PROFILE
/* Entry is driven by the resolved custom keycode, not a fixed matrix cell.
 * Only a real matrix position is accepted. Exit observes that same cell even
 * if its layer/keycode differs after the application restart. */
bool my2k_asset_mode_request_key(uint8_t row,uint8_t col,bool pressed,uint32_t now);
bool my2k_asset_mode_position(uint8_t *row,uint8_t *col);
/* After return to normal mode, discard the initiating key until a complete
 * release. Other physical keys remain usable during this short guard. */
bool my2k_asset_mode_blocks_key(uint8_t row,uint8_t col);
/* Called after the board's real matrix scan and debounce. Raw state prevents
 * a held key from looking released during initial debounce after a reset. */
void my2k_asset_mode_observe(bool raw_pressed,bool debounced_pressed,uint32_t now);
#endif
void my2k_asset_mode_task(uint32_t now);
void my2k_asset_mode_status(uint8_t report[32]);
/* Host-test seams; production implementations remain local to the module. */
#if MY2K_STORAGE_MODE_HOST
uint32_t my2k_storage_platform_marker(void);
void my2k_storage_platform_marker_write(uint32_t marker);
bool my2k_storage_platform_configured(void);
bool my2k_storage_platform_idle(void);
bool my2k_storage_platform_eeprom_idle(void);
bool my2k_storage_platform_ejected(void);
bool my2k_storage_platform_aux_power(bool on);
void my2k_storage_platform_reset(void);
#endif
