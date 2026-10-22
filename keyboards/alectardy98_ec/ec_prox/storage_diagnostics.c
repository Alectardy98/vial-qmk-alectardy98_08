// Copyright 2026 Alectardy98
// SPDX-License-Identifier: GPL-2.0-or-later
/* Compile the exact shared safety implementation, observing its public boundary.
 * No guard is changed, and the raw ChibiOS operations remain behind that guard.
 * This board-only adapter distinguishes initialization from later flash errors.
 */
#define backing_store_init prox_guard_init
#define backing_store_unlock prox_guard_unlock
#define eeprom_driver_init prox_guard_driver_init
#define efl_lld_program prox_guard_program
#define efl_lld_start_erase_sector prox_guard_erase
#define efl_lld_query_erase prox_guard_query
#define efl_lld_start prox_guard_start
#include "../common/runtime/storage.c"
#undef backing_store_init
#undef backing_store_unlock
#undef eeprom_driver_init
#undef efl_lld_program
#undef efl_lld_start_erase_sector
#undef efl_lld_query_erase
#undef efl_lld_start

static uint8_t storage_fault; // first failure: 1 geometry, 2 backend, 3 journal, 4..8 operations
static uint32_t programs, erases;
static flash_error_t last_error;
static void note(uint8_t fault) { if (!storage_fault) storage_fault = fault; }
bool backing_store_init(void) {
    bool valid_geometry = geometry();
    bool ok = prox_guard_init();
    if (!ok) note(valid_geometry ? 2 : 1);
    return ok;
}
void eeprom_driver_init(void) {
    prox_guard_driver_init();
    if (!healthy) note(3);
}
bool backing_store_unlock(void) {
    bool ok = prox_guard_unlock();
    if (!ok) note(4);
    return ok;
}
void efl_lld_start(EFlashDriver *p) {
    prox_guard_start(p);
    if (!healthy) note(5);
}
flash_error_t efl_lld_program(void *p, flash_offset_t offset, size_t n, const uint8_t *data) {
    ++programs;
    last_error = prox_guard_program(p, offset, n, data);
    if (last_error != FLASH_NO_ERROR) note(6);
    return last_error;
}
flash_error_t efl_lld_start_erase_sector(void *p, flash_sector_t sector) {
    ++erases;
    last_error = prox_guard_erase(p, sector);
    if (last_error != FLASH_NO_ERROR) note(7);
    return last_error;
}
flash_error_t efl_lld_query_erase(void *p, uint32_t *wait) {
    last_error = prox_guard_query(p, wait);
    if (last_error != FLASH_NO_ERROR && last_error != FLASH_BUSY_ERASING) note(8);
    return last_error;
}
#ifdef A98_STORAGE_HOST_TEST
uint8_t prox_storage_fault_code(void) { return storage_fault; }
void prox_storage_clear_trace(void) { storage_fault = 0; last_error = FLASH_NO_ERROR; }
#else
#include "quantum.h"
#include "print.h"
void housekeeping_task_kb(void) {
    static uint32_t last;
    static bool printed;
    if ((!printed || !healthy) && timer_elapsed32(last) >= 5000) {
        last = timer_read32(); printed = true;
        const flash_descriptor_t *d = EFLD1.descriptor;
        // Never dereference a descriptor that the exact shared validator rejects.
        bool geometry_ok = geometry();
        uprintf("PROX storage ready=%u fault=%u geometry=%u state=%u error=%u P/E=%lu/%lu\n",
                healthy, storage_fault, geometry_ok, (unsigned)EFLD1.state,
                (unsigned)last_error, (unsigned long)programs, (unsigned long)erases);
        uprintf("factory_KiB=%u OPTCR=%08lx SR=%08lx CR=%08lx desc=%08lx end=%08lx\n",
                a98_storage_density(), (unsigned long)FLASH->OPTCR,
                (unsigned long)FLASH->SR, (unsigned long)FLASH->CR,
                (unsigned long)(uintptr_t)d, (unsigned long)(uintptr_t)A98_IMAGE_END_SYMBOL);
        uintptr_t ptr = (uintptr_t)d, end = (uintptr_t)A98_IMAGE_END_SYMBOL;
        bool trusted = ptr >= A98_TABLE_START && ptr <= end && sizeof(*d) <= end-ptr && ptr%4 == 0;
        if (trusted)
            uprintf("EFL bytes=%lu sectors=%lu page=%lu attrs=%lx\n",
                    (unsigned long)d->size, (unsigned long)d->sectors_count,
                    (unsigned long)d->page_size, (unsigned long)d->attributes);
    }
    housekeeping_task_user();
}
#endif
