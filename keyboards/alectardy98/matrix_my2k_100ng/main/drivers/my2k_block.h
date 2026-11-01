/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_BLOCK_H
#define MY2K_BLOCK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MY2K_BLOCK_SIZE 512u
#define MY2K_BRIDGE_MAX_TRANSFER 4096u
typedef bool (*my2k_block_ready_fn)(void *ctx);
typedef bool (*my2k_block_read_fn)(void *ctx, uint32_t lba, uint8_t *dst, uint32_t blocks);
typedef struct {
    void               *ctx;
    my2k_block_ready_fn ready;
    my2k_block_read_fn  read;
} my2k_block_ops;
typedef enum {
    MY2K_BLOCK_OK = 0,
    MY2K_BLOCK_NO_MEDIA,
    MY2K_BLOCK_RANGE,
    MY2K_BLOCK_IO,
    MY2K_BLOCK_PROTECTED,
    MY2K_BLOCK_ARGUMENT
} my2k_block_error;
typedef struct {
    my2k_block_ops   ops;
    uint32_t         blocks, generation;
    bool             attached;
    my2k_block_error error;
    _Alignas(4) uint8_t scratch[MY2K_BLOCK_SIZE];
} my2k_block_bridge;
/* Read-only exported-LUN adapter. Geometry is an actual backend block COUNT,
 * never SCSI last-LBA. No guessed 121MiB adjustment and no write operation. */
void my2k_block_init(my2k_block_bridge *bridge);
bool my2k_block_attach(my2k_block_bridge *bridge, const my2k_block_ops *ops, uint32_t blocks, uint16_t block_size);
void my2k_block_detach(my2k_block_bridge *bridge);
bool my2k_block_capacity(my2k_block_bridge *bridge, uint32_t *blocks, uint16_t *size);
/* Synchronous bounded core: call on storage worker, not matrix scan/USB ISR.
 * Backend owns bus timeout; callers must serialize access to scratch/context. */
int32_t my2k_block_read10(my2k_block_bridge *bridge, uint32_t lba, uint32_t offset, void *dst, size_t size);
int32_t my2k_block_write10(my2k_block_bridge *bridge, uint32_t lba, uint32_t offset, const void *src, size_t size);
#endif
