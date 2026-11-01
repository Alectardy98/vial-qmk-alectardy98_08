/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_eeprom.h"
#include "eeprom_driver.h"
#include "timer.h"
#include <stdint.h>
#include <string.h>
#if MY2K_DEV_PERSISTENCE
#include "main_persist.h"
#include "keyboard.h"
static uint32_t revision, saving_revision;
#endif
static uint8_t                 image[MY2K_EEPROM_BYTES];
static my2k_journal            journal;
static bool                    attached, dirty;
static uint32_t                changed_at, slots_per_unit;
static my2k_journal_erase_unit erase_unit;
static void                   *erase_context;
static my2k_journal_result     last_result = MY2K_JOURNAL_EMPTY;
__attribute__((weak)) bool     my2k_eeprom_storage(my2k_journal *j, uint32_t *slots, my2k_journal_erase_unit *erase, void **context) {
    (void)j;
    (void)slots;
    (void)erase;
    (void)context;
    return false;
}
#if MY2K_DEV2_PROFILE
__attribute__((noinline,noclone,externally_visible))
#endif
void eeprom_driver_init(void) {
#if MY2K_DEV_PERSISTENCE
    revision=saving_revision=0;
#endif
    memset(image, 0, sizeof image);
    dirty          = false;
    erase_unit     = NULL;
    erase_context  = NULL;
    slots_per_unit = 0;
    attached       = my2k_eeprom_storage(&journal, &slots_per_unit, &erase_unit, &erase_context);
    if (!attached) {
        last_result = MY2K_JOURNAL_EMPTY;
        return;
    }
    if (journal.max_payload != sizeof image) {
        attached    = false;
        last_result = MY2K_JOURNAL_INVALID;
        return;
    }
    size_t length = 0;
    last_result   = my2k_journal_load(&journal, image, sizeof image, &length);
    if (last_result != MY2K_JOURNAL_OK || length != sizeof image) {
        memset(image, 0, sizeof image);
        if (last_result == MY2K_JOURNAL_OK) last_result = MY2K_JOURNAL_INVALID;
        /* A failed read or incompatible schema must never become an automatic
         * overwrite of the only original image. Empty storage is the sole
         * recoverable initialization case. */
        if (last_result != MY2K_JOURNAL_EMPTY) attached = false;
    }
}
static void mark_dirty(void) {
    dirty      = true;
    changed_at = timer_read32();
#if MY2K_DEV_PERSISTENCE
    revision++;
#endif
}
void eeprom_driver_erase(void) {
    memset(image, 0, sizeof image);
    mark_dirty();
}
void eeprom_driver_format(bool erase) {
    if (erase) eeprom_driver_erase();
}
void eeprom_read_block(void *out, const void *address, size_t length) {
    if (!out || !length) return;
    memset(out, 0, length);
    uintptr_t offset = (uintptr_t)address;
    if (offset >= sizeof image) return;
    size_t available = sizeof image - offset;
    if (length > available) length = available;
    memcpy(out, image + offset, length);
}
void eeprom_write_block(const void *data, void *address, size_t length) {
    uintptr_t offset = (uintptr_t)address;
    if (!data || !length || offset >= sizeof image) return;
    size_t available = sizeof image - offset;
    if (length > available) length = available;
    if (memcmp(image + offset, data, length)) {
        memcpy(image + offset, data, length);
        mark_dirty();
    }
}
void my2k_eeprom_task(uint32_t now) {
#if MY2K_DEV_PERSISTENCE
    if (my2k_persist_busy()) {
        int completed=my2k_persist_poll();
        if (completed>0) { last_result=MY2K_JOURNAL_OK; if (saving_revision==revision) dirty=false; }
        else if (completed<0) last_result=MY2K_JOURNAL_IO;
        return;
    }
    if (!attached || !dirty || my2k_persist_error() || (uint32_t)(now-changed_at)<750 || last_input_activity_elapsed()<1000) return;
    saving_revision=revision;
    if (!my2k_persist_begin(image)) last_result=MY2K_JOURNAL_IO;
    else if (!my2k_persist_busy()) { last_result=MY2K_JOURNAL_OK; if (saving_revision==revision) dirty=false; }
#else
    if (!attached || !dirty || (uint32_t)(now - changed_at) < 1000) return;
    last_result = my2k_journal_save(&journal, image, sizeof image);
    if (last_result == MY2K_JOURNAL_FULL && erase_unit) {
        last_result = my2k_journal_rotate(&journal, slots_per_unit, erase_unit, erase_context);
        if (last_result == MY2K_JOURNAL_OK) last_result = my2k_journal_save(&journal, image, sizeof image);
    }
    if (last_result == MY2K_JOURNAL_OK)
        dirty = false;
    else
        changed_at = now; /* bounded retry, never spin on a failing backend */
#endif
}
bool my2k_eeprom_durable(void) {
#if MY2K_DEV_PERSISTENCE
    return attached && !my2k_persist_error();
#else
    return attached;
#endif
}
bool my2k_eeprom_pending(void) { return dirty; }
my2k_journal_result my2k_eeprom_last_result(void) {
    return last_result;
}
