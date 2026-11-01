/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "main_eeprom.h"
bool     my2k_persist_begin(const uint8_t *image);
int      my2k_persist_poll(void); /* 0 busy/idle, 1 committed, -1 latched fault */
bool     my2k_persist_busy(void);
uint32_t my2k_persist_commits(void);
uint32_t my2k_persist_erases(void);
uint32_t my2k_persist_error(void);

#ifndef MY2K_PERSIST_HOST
/* Kept as a guarded ABI boundary so LTO cannot remove address validation. */
bool my2k_settings_flash_operation(uint32_t address, uint32_t value, bool erase);
#endif

#if MY2K_LOCK_LIGHTS_PROFILE
/* Share the single flash-owner fault latch with the settings extension. */
void my2k_persist_extension_fault(void);
#endif
