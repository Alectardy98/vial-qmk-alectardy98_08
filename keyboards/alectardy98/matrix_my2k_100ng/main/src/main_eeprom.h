/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "my2k_journal.h"
#if MY2K_EEPROM32_PROFILE
#    define MY2K_EEPROM_BYTES 32768u
bool my2k_eeprom32_hid(uint8_t *, uint8_t);
/* Physical EE_CLR: queue a whole-Main reset; restart only after durable commit.
 * The AUX/external asset filesystem is outside this storage backend. */
bool my2k_eeprom32_reset_request(void);
bool my2k_eeprom32_reset_pending(void);
bool my2k_eeprom32_reset_ready(void);
#else
#    define MY2K_EEPROM_BYTES 8192u
#endif
/* Override only after an independently reviewed safe storage map exists.
 * Default false installs no backend; no physical read/program/erase occurs. */
bool                my2k_eeprom_storage(my2k_journal *journal, uint32_t *slots_per_unit, my2k_journal_erase_unit *erase,
                                        void **erase_context);
void                my2k_eeprom_task(uint32_t now_ms);
bool                my2k_eeprom_durable(void);
my2k_journal_result my2k_eeprom_last_result(void);
bool                my2k_eeprom_pending(void);
