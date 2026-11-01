/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_factory_defaults.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t flash[2 * MY2K_NV_BANK_BYTES], saved[sizeof flash], image[MY2K_NV_BYTES];
static my2k_nv32 store;
static int cut = -1;
static unsigned words, erases;
static bool read_flash(uint32_t at, void *out, size_t n) {
    assert(at >= MY2K_NV_BASE && at + n <= MY2K_NV_BASE + sizeof flash);
    memcpy(out, flash + at - MY2K_NV_BASE, n); return true;
}
static bool program(uint32_t at, uint32_t value) {
    assert(!(at & 3) && at >= MY2K_NV_BASE && at <= MY2K_NV_BASE + sizeof flash - 4);
    if (cut >= 0 && words++ == (unsigned)cut) return false;
    if (cut < 0) words++;
    uint32_t previous; memcpy(&previous, flash + at - MY2K_NV_BASE, 4);
    assert((previous & value) == value);
    memcpy(flash + at - MY2K_NV_BASE, &value, 4); return true;
}
static bool erase(uint32_t at) {
    assert(at == MY2K_NV_BASE || at == MY2K_NV_BASE + MY2K_NV_BANK_BYTES);
    erases++; memset(flash + at - MY2K_NV_BASE, 255, MY2K_NV_BANK_BYTES); return true;
}
static void reopen(void) {
    assert(my2k_nv_init(&store, (my2k_nv_io){read_flash, program, erase}));
}
static void verify_defaults(void) {
    assert(my2k_nv_read(&store, 0, image, sizeof image));
    assert(!memcmp(image, my2k_factory_image, sizeof image));
}
int main(void) {
    memset(flash, 255, sizeof flash); reopen();
    assert(my2k_factory_seed(&store)); verify_defaults();
    unsigned initialization_words = words;
    memcpy(saved, flash, sizeof flash); reopen(); verify_defaults();
    unsigned before_words = words, before_erases = erases;
    assert(my2k_factory_seed(&store));
    assert(words == before_words && erases == before_erases);
    /* A normal reflash/startup must preserve custom settings, not re-seed. */
    uint8_t changed = (uint8_t)(my2k_factory_image[700] ^ 0xff);
    assert(my2k_nv_write(&store, 700, &changed, 1) && my2k_nv_flush(&store));
    reopen(); before_words = words; assert(my2k_factory_seed(&store));
    assert(words == before_words);
    assert(my2k_nv_read(&store, 700, image, 1) && image[0] == changed);
    /* Clear EEPROM intentionally returns every logical region to defaults. */
    assert(my2k_nv_clear(&store) && my2k_nv_flush(&store)); reopen();
    assert(my2k_factory_seed(&store)); verify_defaults();
    /* Cut every program word in first initialization: a valid marker must
     * never authorize a partial image, and every restart must recover. */
    for (unsigned position = 0; position <= initialization_words; ++position) {
        memset(flash, 255, sizeof flash); cut = -1; reopen();
        cut = (int)position; words = 0; (void)my2k_factory_seed(&store);
        cut = -1; reopen();
        uint8_t magic[2]; assert(my2k_nv_read(&store, 0, magic, 2));
        if (magic[0] == 0xe3 && magic[1] == 0xfe) verify_defaults();
        assert(my2k_factory_seed(&store)); verify_defaults();
    }
    /* Reset/re-seed of an existing bank must also be restart-safe. */
    for (unsigned position = 0; position < initialization_words; position += 31) {
        memcpy(flash, saved, sizeof flash); cut = -1; reopen();
        assert(my2k_nv_clear(&store) && my2k_nv_flush(&store)); reopen();
        cut = (int)position; words = 0; (void)my2k_factory_seed(&store);
        cut = -1; reopen(); assert(my2k_factory_seed(&store)); verify_defaults();
    }
    printf("PASS factory snapshot, no-write existing settings, reset, %u first-init power cuts, reset/reseed cuts\n", initialization_words + 1);
    return 0;
}
