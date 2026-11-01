/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_block.h"
#include <string.h>
void my2k_block_init(my2k_block_bridge *b) {
    if (b)
        memset(b, 0, sizeof(*b));
}
void my2k_block_detach(my2k_block_bridge *b) {
    if (!b)
        return;
    b->attached = false;
    b->blocks   = 0;
    b->generation++;
    b->error = MY2K_BLOCK_NO_MEDIA;
    memset(&b->ops, 0, sizeof(b->ops));
}
bool my2k_block_attach(my2k_block_bridge *b, const my2k_block_ops *ops, uint32_t blocks, uint16_t size) {
    if (!b)
        return false;
    my2k_block_detach(b);
    if (!ops || !ops->ready || !ops->read || !blocks || size != MY2K_BLOCK_SIZE) {
        b->error = MY2K_BLOCK_ARGUMENT;
        return false;
    }
    b->ops      = *ops;
    b->blocks   = blocks;
    b->attached = true;
    b->error    = MY2K_BLOCK_OK;
    return true;
}
static bool present(my2k_block_bridge *b) {
    if (!b || !b->attached || !b->ops.ready || !b->ops.ready(b->ops.ctx)) {
        if (b)
            b->error = MY2K_BLOCK_NO_MEDIA;
        return false;
    }
    return true;
}
bool my2k_block_capacity(my2k_block_bridge *b, uint32_t *blocks, uint16_t *size) {
    if (!blocks || !size) {
        if (b)
            b->error = MY2K_BLOCK_ARGUMENT;
        return false;
    }
    *blocks = 0;
    *size   = 0;
    if (!present(b))
        return false;
    *blocks  = b->blocks;
    *size    = MY2K_BLOCK_SIZE;
    b->error = MY2K_BLOCK_OK;
    return true;
}
int32_t my2k_block_read10(my2k_block_bridge *b, uint32_t lba, uint32_t offset, void *dst, size_t size) {
    if (!b)
        return -1;
    if ((!dst && size) || offset >= MY2K_BLOCK_SIZE || size > MY2K_BRIDGE_MAX_TRANSFER) {
        b->error = MY2K_BLOCK_ARGUMENT;
        return -1;
    }
    if (!present(b))
        return -1;
    uint64_t start = (uint64_t)lba * MY2K_BLOCK_SIZE + offset;
    uint64_t cap   = (uint64_t)b->blocks * MY2K_BLOCK_SIZE;
    if (start > cap || (uint64_t)size > cap - start) {
        b->error = MY2K_BLOCK_RANGE;
        return -1;
    }
    uint32_t generation = b->generation;
#if MY2K_DEV15_PROFILE
    if (size && !offset && !(size % MY2K_BLOCK_SIZE) && !((uintptr_t)dst & 3u)) {
        if (!b->ops.read(b->ops.ctx, lba, dst, (uint32_t)(size / MY2K_BLOCK_SIZE))) {
            b->error = MY2K_BLOCK_IO;
            return -1;
        }
        if (b->generation != generation || !b->attached) {
            b->error = MY2K_BLOCK_NO_MEDIA;
            return -1;
        }
        b->error = MY2K_BLOCK_OK;
        return (int32_t)size;
    }
#endif
    uint8_t *out  = dst;
    size_t   left = size;
    while (left) {
        if (!present(b) || b->generation != generation) {
            b->error = MY2K_BLOCK_NO_MEDIA;
            return -1;
        }
        if (!b->ops.read(b->ops.ctx, lba, b->scratch, 1)) {
            b->error = MY2K_BLOCK_IO;
            return -1;
        }
        if (b->generation != generation || !b->attached) {
            b->error = MY2K_BLOCK_NO_MEDIA;
            return -1;
        }
        size_t n = MY2K_BLOCK_SIZE - offset;
        if (n > left)
            n = left;
        memcpy(out, b->scratch + offset, n);
        out += n;
        left -= n;
        lba++;
        offset = 0;
    }
    b->error = MY2K_BLOCK_OK;
    return (int32_t)size;
}
int32_t my2k_block_write10(my2k_block_bridge *b, uint32_t lba, uint32_t offset, const void *src, size_t size) {
    (void)lba;
    (void)offset;
    (void)src;
    (void)size;
    if (b)
        b->error = MY2K_BLOCK_PROTECTED;
    return -1;
}
