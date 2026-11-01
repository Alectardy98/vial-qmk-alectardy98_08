/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_msc.h"
#include "aux_platform.h"
#include <limits.h>
int32_t aux_msc_read(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t size) {
    uint32_t sectors = aux_sd_sectors();
    if (lun || !buffer || size > INT32_MAX || !sectors || lba >= sectors || offset >= 512u)
        return -1;
    uint64_t start = (uint64_t)lba * 512u + offset;
    if (size > (uint64_t)sectors * 512u - start)
        return -1;
    uint8_t  block[512], *out = buffer;
    uint32_t remaining = size;
    while (remaining) {
#if MY2K_DEV15_PROFILE
        /* Aligned full sectors need no scratch copy. SD remains the existing
         * single-sector, cooperative read path; do not invent CMD18 or DMA. */
        if (!offset && remaining >= 512u) {
#    if MY2K_SD_BULK8
            unsigned blocks = remaining / 512u;
            if (blocks > 8u)
                blocks = 8u;
            if (!aux_sd_read_blocks(lba, out, blocks))
                return -1;
            out += 512u * blocks;
            remaining -= 512u * blocks;
            lba += blocks;
            continue;
#    else
            if (!aux_sd_read(lba, out))
                return -1;
            out += 512u;
            remaining -= 512u;
            lba++;
            continue;
#    endif
        }
#endif
        if (!aux_sd_read(lba, block))
            return -1;
        uint32_t count = 512u - offset;
        if (count > remaining)
            count = remaining;
        for (uint32_t i = 0; i < count; i++)
            out[i] = block[offset + i];
        out += count;
        remaining -= count;
        lba++;
        offset = 0;
    }
    return (int32_t)size;
}
#if MY2K_STOCK_PARITY_PROFILE
int32_t aux_msc_write(uint8_t lun, uint32_t lba, uint32_t offset, const void *buffer, uint32_t size) {
#    if MY2K_DEV7_PROFILE && !MY2K_RELEASE1_PROFILE
    (void)lun;
    (void)lba;
    (void)offset;
    (void)buffer;
    (void)size;
    return -1; /* Dev7 isolates read reliability; no asset write backend. */
#    else
    uint32_t sectors = aux_sd_sectors();
    if (lun || !buffer || !size || size > 512 || !sectors || lba >= sectors || offset >= 512u ||
        size > (uint64_t)(sectors - lba) * 512u - offset)
        return -1;
    uint8_t        block[512];
    const uint8_t *in        = buffer;
    uint32_t       remaining = size;
    while (remaining) {
        uint32_t n = 512u - offset;
        if (n > remaining)
            n = remaining;
        if ((offset || n != 512u) && !aux_sd_read(lba, block))
            return -1;
        for (uint32_t i = 0; i < n; i++)
            block[offset + i] = in[i];
        if (!aux_sd_write(lba, block))
            return -1;
        remaining -= n;
        in += n;
        lba++;
        offset = 0;
    }
    return (int32_t)size;
#    endif
}
#endif
