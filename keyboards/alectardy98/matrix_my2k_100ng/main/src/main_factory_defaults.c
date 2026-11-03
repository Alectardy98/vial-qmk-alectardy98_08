/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_factory_defaults.h"
#include <string.h>
#ifndef MY2K_FACTORY_IMAGE_INCLUDE
#    define MY2K_FACTORY_IMAGE_INCLUDE "defaults/factory.inc"
#endif
#include MY2K_FACTORY_IMAGE_INCLUDE

bool my2k_factory_seed(my2k_nv32 *store) {
    uint8_t magic[2];
    if (!my2k_nv_read(store, 0, magic, sizeof magic))
        return false;
    /* A valid existing EEPROM is never replaced by compiled defaults. */
    if (magic[0] == 0xe3 && magic[1] == 0xfe)
        return true;
    if (!my2k_nv_clear(store))
        return false;
    /* Publish the validity marker LAST. Any interruption before its commit
     * leaves an invalid image and the next startup repeats initialization.
     * The existing journal keeps each page batch atomic across power loss. */
    uint8_t page[MY2K_NV_PAGE];
    for (unsigned at = 0; at < MY2K_NV_BYTES; at += sizeof page) {
        memcpy(page, my2k_factory_image + at, sizeof page);
        if (!at)
            page[0] = page[1] = 0;
        bool nonzero = false;
        for (unsigned i = 0; i < sizeof page; ++i)
            nonzero |= page[i] != 0;
        if (nonzero && !my2k_nv_write(store, at, page, sizeof page))
            return false;
    }
    if (!my2k_nv_flush(store))
        return false;
    return my2k_nv_write(store, 0, my2k_factory_image, 2) && my2k_nv_flush(store);
}
