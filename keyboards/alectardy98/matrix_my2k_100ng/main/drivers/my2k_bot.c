/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_bot.h"
#include <string.h>
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t command_bytes(const uint8_t *c, size_t n) {
    if (!n)
        return 0;
    switch (c[0]) {
        case 0x12:
            return n == 6 ? (c[4] < 36 ? c[4] : 36) : 0;
        case 3:
            return n == 6 ? (c[4] < 18 ? c[4] : 18) : 0;
        case 0x1a:
            return n == 6 ? (c[4] < 4 ? c[4] : 4) : 0;
        case 0x25:
            return n == 10 ? 8 : 0;
        case 0x28:
#if MY2K_DEV7_WRITABLE_MSC
        case 0x2a:
#endif
            return n == 10 ? ((uint32_t)c[7] * 256 + c[8]) * 512 : 0;
        default:
            return 0;
    }
}
static void status(my2k_bot *b, uint8_t code) {
    b->status  = code;
    b->stage   = MY2K_BOT_STATUS;
    b->pending = 0;
    if (b->transferred < b->host_length) {
        if (b->stall_out
#if MY2K_DEV7_WRITABLE_MSC
            || !b->host_in
#endif
        )
            b->stall_out = true;
        else
            b->stall_in = true;
    }
    put32(b->csw, 0x53425355);
    put32(b->csw + 4, b->tag);
    put32(b->csw + 8, b->host_length - b->transferred);
    b->csw[12] = code;
}
void my2k_bot_init(my2k_bot *b, my2k_scsi *s) {
    if (b) {
        memset(b, 0, sizeof(*b));
        b->scsi = s;
    }
}
bool my2k_bot_wants_out(const my2k_bot *b) {
    return b &&
           (b->stage == MY2K_BOT_CBW
#if MY2K_DEV7_WRITABLE_MSC
            || (b->stage == MY2K_BOT_OUT && b->scsi->state == MY2K_SCSI_OUT)
#endif
                ) &&
           !b->stall_out && b->scsi && b->scsi->state != MY2K_SCSI_DRAIN;
}
void my2k_bot_out(my2k_bot *b, const uint8_t *p, size_t n, uint32_t now) {
    if (!b || !b->scsi)
        return;
#if MY2K_DEV7_WRITABLE_MSC
    if (b->stage == MY2K_BOT_OUT) {
        if (!my2k_scsi_out(b->scsi, p, n)) {
            b->stall_out = true;
            status(b, 2);
            return;
        }
        b->transferred += (uint32_t)n;
        b->started_ms = now;
        if (b->scsi->state == MY2K_SCSI_WAIT)
            b->stage = MY2K_BOT_COMMAND;
        my2k_bot_poll(b, now);
        return;
    }
#endif
    if (!my2k_bot_wants_out(b) || !p || n != 31 || le32(p) != 0x43425355 || (p[12] & 0x7f) || p[13] || !p[14] ||
        p[14] > 16) {
        b->stage    = MY2K_BOT_RECOVERY;
        b->stall_in = b->stall_out = true;
        b->reset_seen              = false;
        b->clear_mask              = 0;
        return;
    }
    b->tag         = le32(p + 4);
    b->host_length = le32(p + 8);
    b->transferred = 0;
    b->offset      = 0;
    b->pending     = 0;
    b->started_ms  = now;
    bool in        = (p[12] & 0x80) != 0;
#if MY2K_DEV7_WRITABLE_MSC
    b->host_in = in;
#endif
    uint32_t needed = command_bytes(p + 15, p[14]);
    /* This bridge implements no command requiring OUT data. Reject all such data
     * without accepting it or calling a media write. Host clears halt for CSW. */
#if MY2K_DEV7_WRITABLE_MSC
    if (p[15] == 0x2a && b->host_length && !in) {
        if (!my2k_scsi_start(b->scsi, p + 15, p[14], false, b->host_length)) {
            status(b, 1);
            return;
        }
        if (b->scsi->state == MY2K_SCSI_OUT)
            b->stage = MY2K_BOT_OUT;
        else if (b->scsi->state == MY2K_SCSI_FAILED) {
            b->stall_out = true;
            status(b, 1);
        } else
            status(b, 2);
        return;
    }
#endif
    if (b->host_length && !in) {
        /* Record the actual rejection sense (e.g. DATA PROTECT for WRITE10);
         * this read-only service has no mutation callback. */
        (void)my2k_scsi_start(b->scsi, p + 15, p[14], false, b->host_length);
        b->stall_out = true;
        status(b, needed ? 2 : 1);
        return;
    }
    if ((needed && b->host_length < needed) || (!needed && b->host_length && p[15] == 0)) {
        status(b, 2);
        return;
    }
    if (!my2k_scsi_start(b->scsi, p + 15, p[14], in, needed)) {
        status(b, 1);
        return;
    }
    b->stage = MY2K_BOT_COMMAND;
    my2k_bot_poll(b, now);
}
void my2k_bot_poll(my2k_bot *b, uint32_t now) {
    if (!b || !b->scsi)
        return;
    if (b->scsi->state == MY2K_SCSI_DRAIN)
        (void)my2k_scsi_poll(b->scsi);
#if MY2K_DEV15_PROFILE
    if (b->stage == MY2K_BOT_DATA)
        (void)my2k_scsi_prefetch(b->scsi);
#endif
#if MY2K_DEV7_WRITABLE_MSC
    if (b->stage == MY2K_BOT_OUT && (uint32_t)(now - b->started_ms) > 3000u) {
        my2k_scsi_cancel(b->scsi);
        b->stall_out = true;
        status(b, 1);
        return;
    }
#endif
    if (b->stage != MY2K_BOT_COMMAND)
        return;
    if ((uint32_t)(now - b->started_ms) > 3000) {
        my2k_scsi_cancel(b->scsi);
        status(b, 1);
        return;
    }
    my2k_scsi_state state = my2k_scsi_poll(b->scsi);
    if (state == MY2K_SCSI_DATA)
        b->stage = MY2K_BOT_DATA;
    else if (state == MY2K_SCSI_PASSED)
        status(b, 0);
    else if (state == MY2K_SCSI_FAILED)
        status(b, 1);
#if MY2K_DEV7_WRITABLE_MSC
    else if (state == MY2K_SCSI_OUT)
        b->stage = MY2K_BOT_OUT;
#endif
}
const uint8_t *my2k_bot_peek(my2k_bot *b, uint16_t *n) {
    if (!b || !n || b->stall_in || b->stall_out || b->pending)
        return NULL;
    if (b->stage == MY2K_BOT_STATUS) {
        b->pending        = 13;
        b->pending_status = true;
        *n                = 13;
        return b->csw;
    }
    if (b->stage != MY2K_BOT_DATA)
        return NULL;
    uint16_t       size = 0;
    const uint8_t *data = my2k_scsi_data(b->scsi, &size);
    if (!data || b->offset >= size)
        return NULL;
    uint16_t amount = (uint16_t)(size - b->offset);
#if !MY2K_DEV15_PROFILE
    if (amount > 64)
        amount = 64;
#endif
    if (amount > b->host_length - b->transferred)
        amount = (uint16_t)(b->host_length - b->transferred);
    if (!amount)
        return NULL;
    b->pending        = amount;
    b->pending_status = false;
    *n                = amount;
    return data + b->offset;
}
void my2k_bot_in_complete(my2k_bot *b) {
    if (!b || !b->pending)
        return;
    if (b->pending_status) {
        b->pending = 0;
        b->stage   = b->status == 2 ? MY2K_BOT_RECOVERY : MY2K_BOT_CBW;
        if (b->status == 2) {
            b->stall_in = b->stall_out = true;
            b->reset_seen              = false;
            b->clear_mask              = 0;
        }
        return;
    }
    b->transferred += b->pending;
    b->offset += b->pending;
    b->pending    = 0;
    uint16_t size = 0;
    (void)my2k_scsi_data(b->scsi, &size);
    if (b->offset == size) {
        my2k_scsi_consume(b->scsi);
        b->offset = 0;
        b->stage  = MY2K_BOT_COMMAND;
    }
}
void my2k_bot_class_reset(my2k_bot *b) {
    if (!b)
        return;
    my2k_scsi_cancel(b->scsi);
    b->pending    = 0;
    b->stage      = MY2K_BOT_RECOVERY;
    b->reset_seen = true;
    b->clear_mask = 0;
}
void my2k_bot_clear_halt(my2k_bot *b, bool in) {
    if (!b)
        return;
    if (b->stage == MY2K_BOT_RECOVERY && !b->reset_seen)
        return;
    if (in)
        b->stall_in = false;
    else
        b->stall_out = false;
    if (b->stage == MY2K_BOT_RECOVERY) {
        b->clear_mask |= in ? 1 : 2;
        if (b->clear_mask == 3) {
            b->stage      = MY2K_BOT_CBW;
            b->reset_seen = false;
        }
    }
}
void my2k_bot_disconnect(my2k_bot *b) {
    if (!b)
        return;
    my2k_scsi *s = b->scsi;
    my2k_scsi_cancel(s);
    my2k_bot_init(b, s);
}
