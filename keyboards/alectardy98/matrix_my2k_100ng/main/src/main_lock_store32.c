/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_lock_store.h"
#include "main_eeprom.h"
#include "main_persist.h"
#include "eeprom_driver.h"
#include <string.h>
#define ADDRESS ((void *)(uintptr_t)32512u)
static uint32_t commits;
static bool     pending;
void            my2k_lock_store_init(void) {
    commits = 0;
    pending = false;
}
bool my2k_lock_store_load(uint8_t *p) {
    uint8_t saved[MY2K_LOCK_BYTES];
    if (!p)
        return false;
    eeprom_read_block(saved, ADDRESS, sizeof saved);
    if (!my2k_lock_validate(saved))
        return false;
    memcpy(p, saved, sizeof saved);
    return true;
}
bool my2k_lock_store_request(const uint8_t *p) {
    if (!my2k_lock_validate(p) || !my2k_eeprom_durable())
        return false;
    eeprom_write_block(p, ADDRESS, MY2K_LOCK_BYTES);
    pending = my2k_eeprom_pending();
    return !my2k_persist_error();
}
void my2k_lock_store_task(void) {
    if (pending && !my2k_eeprom_pending()) {
        pending = false;
        commits++;
    }
}
void my2k_lock_store_erased(void) {} /* No sector4 coupling in this profile. */
bool my2k_lock_store_pending(void) {
    return my2k_eeprom_pending();
}
uint32_t my2k_lock_store_error(void) {
    return my2k_persist_error();
}
uint16_t my2k_lock_store_remaining(void) {
    return UINT16_MAX;
} /* Rotating store, not a fixed save limit. */
uint32_t my2k_lock_store_commits(void) {
    return commits;
}
