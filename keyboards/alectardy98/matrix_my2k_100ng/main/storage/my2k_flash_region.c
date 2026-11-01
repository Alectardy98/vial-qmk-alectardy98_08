/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_flash_region.h"
#define SECTOR_BYTES UINT32_C(0x20000)
/* APM32F405xG candidate geometry, RM V2.4 table 8, pending library/errata
 * verification. No actual target enables this allocation or supplies MMIO. */
static uint32_t sector_base(uint8_t s) {
    return UINT32_C(0x08040000) + (uint32_t)(s - 6) * SECTOR_BYTES;
}
static bool location(my2k_flash_region *r, uint32_t offset, size_t n, uint32_t *address) {
    uint32_t used = r->slots_per_sector * r->slot_bytes;
    if (offset >= r->virtual_bytes || n > r->virtual_bytes - offset) return false;
    uint32_t unit = offset / used, within = offset % used;
    if (n > used - within) return false;
    *address = sector_base((uint8_t)(r->first_sector + unit)) + within;
    return true;
}
static bool read_region(void *ctx, uint32_t offset, void *out, size_t n) {
    my2k_flash_region *r = ctx;
    uint32_t           address;
    if (!n) return offset <= r->virtual_bytes;
    return location(r, offset, n, &address) && r->io.read(r->io.context, address, out, n);
}
static bool program_region(void *ctx, uint32_t offset, const void *data, size_t n) {
    my2k_flash_region *r = ctx;
    uint32_t           address;
    if (offset % 4 || n % 4 || !data || !location(r, offset, n, &address)) return false;
    const uint8_t *p = data;
    for (size_t i = 0; i < n; i += 4)
        if (!r->io.program_word(r->io.context, address + (uint32_t)i, p + i)) return false;
    return true;
}
bool my2k_flash_region_init(my2k_flash_region *r, my2k_flash_io io, uint8_t first, uint8_t count, uint32_t payload, bool reviewed, my2k_journal *j) {
    if (!r || !j || !reviewed || !io.read || !io.program_word || !io.erase_sector || first < 6 || first > 10 || count < 2 || count > 12 - first || !payload || payload > SECTOR_BYTES - 23) return false;
    uint32_t slot = (payload + 23) & ~3u, slots = SECTOR_BYTES / slot;
    *r = (my2k_flash_region){io, first, count, slots, slot, slots * slot * count};
    return my2k_journal_init(j, (my2k_journal_backend){r, r->virtual_bytes, read_region, program_region}, payload);
}
bool my2k_flash_region_erase(void *ctx, uint32_t unit) {
    my2k_flash_region *r = ctx;
    if (!r || unit >= r->sector_count) return false;
    return r->io.erase_sector(r->io.context, (uint8_t)(r->first_sector + unit));
}
