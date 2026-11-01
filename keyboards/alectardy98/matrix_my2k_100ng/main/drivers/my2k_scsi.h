/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_SCSI_H
#define MY2K_SCSI_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Read-only, single-LUN SCSI command service. No USB endpoint or BOT recovery
 * is implied by this API. Backend callbacks MUST be non-blocking. */
typedef struct {
    void *context;
    bool (*capacity)(void *, uint32_t *blocks, uint16_t *size, uint32_t *generation);
    bool (*submit)(void *, uint32_t lba, uint16_t bytes);
    bool (*take)(void *, void *dst, size_t capacity, int32_t *result);
#if MY2K_DEV15_PROFILE
    uint16_t (*read_limit)(void *); /* Negotiated whole-sector bytes, max4096. */
#endif
#if MY2K_DEV7_WRITABLE_MSC
    /* Explicit asset-export ownership + exact AUX pairing gate in backend.
     * Source remains owned/stable until take reports512; sync reports0. */
    bool (*writable)(void *);
    bool (*submit_write)(void *, uint32_t lba, const uint8_t data[512]);
    bool (*submit_sync)(void *);
#endif
} my2k_scsi_ops;
typedef enum { MY2K_SCSI_IDLE, MY2K_SCSI_DATA, MY2K_SCSI_WAIT, MY2K_SCSI_DRAIN, MY2K_SCSI_PASSED, MY2K_SCSI_FAILED
#if MY2K_DEV7_WRITABLE_MSC
    ,MY2K_SCSI_OUT
#endif
} my2k_scsi_state;
typedef struct {
    my2k_scsi_ops   ops;
    my2k_scsi_state state;
    uint8_t         sense_key, sense_asc;
    uint32_t        lba, blocks_left, generation;
    uint16_t        data_size;
    bool            inflight;
#if MY2K_DEV7_WRITABLE_MSC
    bool write_command,sync_command,eject_command,ejected,writes_armed;
#endif
#if MY2K_DEV15_PROFILE
    uint16_t read_bytes;
    _Alignas(4) uint8_t data[4096];
#else
    _Alignas(4) uint8_t data[512];
#endif
} my2k_scsi;
void my2k_scsi_init(my2k_scsi *, const my2k_scsi_ops *);
/* New command only after IDLE/PASSED/FAILED. Exact CDB length is checked.
 * 'in' is host direction (ignored for zero-length transfers); transfer_bytes
 * is the host data length. No write/start-stop/reset/format action exists. */
bool            my2k_scsi_start(my2k_scsi *, const uint8_t *cdb, size_t cdb_size, bool in, uint32_t transfer_bytes);
my2k_scsi_state my2k_scsi_poll(my2k_scsi *);
#if MY2K_DEV15_PROFILE
/* At most one next chunk. Never mutates the current DATA state/buffer or
 * reports failure while the USB controller still owns the current transfer. */
bool my2k_scsi_prefetch(my2k_scsi *);
#endif
/* DATA remains stable until consumed, including repeated/partial USB sends. */
const uint8_t *my2k_scsi_data(const my2k_scsi *, uint16_t *size);
void           my2k_scsi_consume(my2k_scsi *);
/* Drops pending output and drains any outstanding backend read before allowing
 * another command. Does not reset USB, media, controller or protection. */
void my2k_scsi_cancel(my2k_scsi *);
#if MY2K_DEV7_WRITABLE_MSC
/* At most64bytes, never past the current512byte block. No backend call until
 * a complete block is owned. Caller must stop OUT while state is WAIT/DRAIN. */
bool my2k_scsi_out(my2k_scsi *,const uint8_t *,size_t);
bool my2k_scsi_ejected(const my2k_scsi *);
/* Read-only/no-media export may leave without an unsupported physical SYNC.
 * Once writable capability was advertised/accepted, only completed eject is safe. */
#endif
#if MY2K_DEV7_PROFILE || MY2K_DEV7_WRITABLE_MSC
bool my2k_scsi_export_exit_safe(const my2k_scsi *);
#endif
#endif
