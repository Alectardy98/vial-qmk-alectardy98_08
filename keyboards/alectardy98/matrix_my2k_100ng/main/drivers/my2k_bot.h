/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_BOT_H
#define MY2K_BOT_H
#include "my2k_scsi.h"
/* Single-LUN read-only Bulk-Only Transport, task context only. USB glue owns
 * endpoint transfers/stalls; this engine never touches a device or registers. */
typedef enum {
    MY2K_BOT_CBW,
    MY2K_BOT_COMMAND,
    MY2K_BOT_DATA,
    MY2K_BOT_STATUS,
    MY2K_BOT_RECOVERY
#if MY2K_DEV7_WRITABLE_MSC
    ,
    MY2K_BOT_OUT
#endif
} my2k_bot_stage;
typedef struct {
    my2k_scsi     *scsi;
    my2k_bot_stage stage;
    uint32_t       tag, host_length, transferred, started_ms;
    uint16_t       offset, pending;
    uint8_t        status, clear_mask;
    bool           stall_in, stall_out, reset_seen, pending_status;
#if MY2K_DEV7_WRITABLE_MSC
    bool host_in;
#endif
    uint8_t csw[13];
} my2k_bot;
void my2k_bot_init(my2k_bot *, my2k_scsi *);
/* Feed one completed OUT transfer. Invalid CBW stalls until BOT reset recovery. */
void my2k_bot_out(my2k_bot *, const uint8_t *, size_t, uint32_t now_ms);
void my2k_bot_poll(my2k_bot *, uint32_t now_ms);
bool my2k_bot_wants_out(const my2k_bot *);
/* Dev15: a whole bounded buffered chunk, otherwise at most64 bytes.
 * USB endpoint packet size stays64; valid until complete/cancel/reset. */
const uint8_t *my2k_bot_peek(my2k_bot *, uint16_t *size);
void           my2k_bot_in_complete(my2k_bot *);
/* These change transport state only, never reset/erase the MCU or media. */
void my2k_bot_class_reset(my2k_bot *);
void my2k_bot_clear_halt(my2k_bot *, bool in);
void my2k_bot_disconnect(my2k_bot *);
#endif
