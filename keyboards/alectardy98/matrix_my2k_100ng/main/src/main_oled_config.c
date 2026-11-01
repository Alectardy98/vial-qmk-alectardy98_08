/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_oled_config.h"
#include "my2k_oled_config.h"
#include "aux_link.h"
#include "main_storage_mode.h"
#include "my2k_usbh_chibios.h"
#include "timer.h"
#include <string.h>
static uint8_t  current[MY2K_OLED_BYTES], staged[MY2K_OLED_BYTES], state, error, token, op, offset;
static uint16_t next;
static bool     peer, waiting, reading, loaded, stored, upload, export_owned;
static uint32_t sent_at, probe_at, applies, upload_at;
static uint8_t  expected[8];
static bool     send_request(unsigned operation, unsigned at) {
    my2k_wire_message m = {.type = MY2K_AC, .v = {operation, token, at / 4u, 0}};
    if (operation == 1) {
        m.v[2] = 120;
        m.v[3] = 1;
    }
    if (operation == 2) {
        m.v[3] = 4;
        memcpy(m.v + 4, staged + at, 4);
    }
    if (!my2k_aux_send(&m)) return false;
    op      = operation;
    offset  = m.v[2];
    waiting = true;
    sent_at = timer_read32();
    return true;
}
__attribute__((noinline, noclone, externally_visible)) void my2k_oled_main_init(void) {
    my2k_oled_defaults(current);
    state = 0;
    peer = waiting = upload = reading = loaded = stored = export_owned = false;
    next = token = op = offset = error = 0;
    probe_at = sent_at = 0;
}
__attribute__((noinline, noclone, externally_visible)) void my2k_oled_main_receive(const my2k_wire_message *m) {
    if (!m || m->type != MY2K_AR || !waiting || m->v[0] != op || m->v[1] != token || m->v[2] != offset) return;
    waiting = false;
    if (m->v[3]) {
        state = 3;
        error = (uint8_t)(16u+m->v[3]);
        return;
    }
    if (op == 6) {
        export_owned = m->v[4] == 1;
        return;
    }
    if (op == 0) {
        peer = m->v[4] == 1 && (m->v[5]&31)==31 && m->v[6] == 72 && m->v[7] == 40;
        if (!peer) {
            state = 3;
            error = 3;
        } else {
            stored  = (m->v[5] & 128) != 0;
            reading = true;
            next    = 0;
            state   = 1;
        }
        return;
    }
    if (op == 5) {
        memcpy(current + offset * 4u, m->v + 4, 4);
        next = (uint16_t)(offset + 1u) * 4u;
        if (next == MY2K_OLED_BYTES) {
            reading = false;
            loaded  = my2k_oled_valid(current);
            state   = loaded ? 2 : 3;
            error   = loaded ? 0 : 4;
        }
        return;
    }
    if (op == 1) {
        next = 0;
        return;
    }
    if (op == 2) {
        next = (uint16_t)(offset + 1u) * 4u;
        return;
    }
    if (op == 3) {
        if (memcmp(m->v + 4, staged + MY2K_OLED_CRC_OFFSET, 4)) {
            state = 3;
            error = 4;
            return;
        }
        memcpy(current, staged, MY2K_OLED_BYTES);
        stored = true;
        my2k_usbh_files_changed();
        applies++;
        state = 2;
        error = 0;
        return;
    }
    if (op == 4) {
        if (memcmp(m->v + 4, expected, 4)) {
            state = 3;
            error = 4;
        } else {
            state = 2;
            error = 0;
        }
        return;
    }
}
__attribute__((noinline, noclone, externally_visible)) void my2k_oled_main_service(uint32_t now) {
    if (upload && (uint32_t)(now - upload_at) > 3000u) {
        upload = false;
        error  = 6;
    }
    if (my2k_asset_export_active()) {
        if (waiting && (uint32_t)(now - sent_at) > 6000u) {
            waiting = false;
            error   = 1;
        }
        if (!waiting && !export_owned && my2k_aux_responsive() && (uint32_t)(now - probe_at) >= 1000u) {
            probe_at = now;
            if (++token == 0) ++token;
            (void)send_request(6, 4);
        }
        return;
    }
    if (my2k_asset_transition_pending()) return;
    if (!my2k_aux_responsive()) {
        peer    = false;
        loaded  = false;
        reading = false;
        if (state == 1) {
            state = 3;
            error = 5;
        }
        waiting = false;
        return;
    }
    /* AFS has already claimed the unique AUX filesystem owner. Do not
     * start/restart OLED configuration discovery halfway through an upload.
     * In-flight ACKs still retire in receive(); admission counts waiting. */
    if (my2k_usbh_animation_busy()) return;
    if (waiting) {
        if ((uint32_t)(now - sent_at) > 6000u) {
            waiting = false;
            state   = 3;
            error   = 1;
        }
        return;
    }
    if (!peer) {
        if ((uint32_t)(now - probe_at) >= 2000u) {
            probe_at = now;
            if (++token == 0) ++token;
            (void)send_request(0, 0);
        }
        return;
    }
    if (state != 1) return;
    if (reading) {
        if (!send_request(5, next)) {
            state = 3;
            error = 5;
        }
        return;
    }
    if (op == 0) {
        if (!send_request(1, 0)) {
            state = 3;
            error = 5;
        }
    } else if (op == 1 || op == 2) {
        if (next < MY2K_OLED_BYTES) {
            if (!send_request(2, next)) {
                state = 3;
                error = 5;
            }
        } else if (!send_request(3, 0)) {
            state = 3;
            error = 5;
        }
    }
}
__attribute__((noinline, noclone, externally_visible)) bool my2k_oled_hid(uint8_t *r, unsigned length) {
    if (length != 32 || r[0] != 0xfd || r[1] != 0xe8) return false;
    uint8_t q[32];
    memcpy(q, r, 32);
    memset(r + 2, 0, 30);
    r[2]             = 0x55;
    r[3]             = 1;
    unsigned command = q[2];
    r[4]             = command;
    if (command == 0) {
        r[2]  = 0xaa;
        r[5]  = peer && loaded;
        r[6]  = 31;
        r[7]  = 72;
        r[8]  = 40;
        r[9]  = 11;
        r[10] = 5;
        r[11] = 10;
        r[12] = MY2K_OLED_BYTES & 255;
        r[13] = MY2K_OLED_BYTES >> 8;
        r[14] = 1;
        return true;
    }
    if (command == 1) {
        unsigned at = q[3] | (unsigned)q[4] << 8;
        if (at >= MY2K_OLED_BYTES) return true;
        unsigned n = MY2K_OLED_BYTES - at;
        if (n > 24) n = 24;
        r[2] = 0xaa;
        r[5] = at;
        r[6] = at >> 8;
        r[7] = n;
        memcpy(r + 8, current + at, n);
        return true;
    }
    if (command == 5) {
        r[2] = 0xaa;
        r[5] = state;
        r[6] = peer && loaded;
        r[7] = error;
        memcpy(r + 8, current + MY2K_OLED_CRC_OFFSET, 4);
        r[12] = state == 1;
        r[13] = stored;
        for (unsigned i = 0; i < 4; i++)
            r[16 + i] = (uint8_t)(applies >> (i * 8));
        return true;
    }
    if (my2k_usbh_animation_busy() || !peer || !loaded || my2k_asset_export_active() || my2k_asset_transition_pending() || state == 1 || waiting) return true;
    if(command==6){reading=true;next=0;state=1;op=0;if(++token==0)++token;r[2]=0xaa;return true;}
    if (command == 2) {
        if (q[3] != 1) return true;
        memset(staged, 0, MY2K_OLED_BYTES);
        next      = 0;
        upload    = true;
        upload_at = timer_read32();
        r[2]      = 0xaa;
        return true;
    }
    if (command == 3) {
        unsigned at = q[3] | (unsigned)q[4] << 8, n = q[5];
        if (!upload || !n || n > 24 || at != next || at + n > MY2K_OLED_BYTES) return true;
        memcpy(staged + at, q + 6, n);
        upload_at = timer_read32();
        next += n;
        r[2] = 0xaa;
        return true;
    }
    if (command == 4) {
        if (!upload || next != MY2K_OLED_BYTES || !my2k_oled_valid(staged)) return true;
        upload = false;
        next   = 0;
        state  = 1;
        error  = 0;
        op     = 0;
        if (++token == 0) ++token;
        r[2] = 0xaa;
        return true;
    }
    if (command == 7) {
        my2k_wire_message tm = {.type = MY2K_TM, .year = (uint16_t)(q[3] | (uint16_t)q[4] << 8)};
        memcpy(tm.v, q + 5, 6);
        char b[128];
        if (tm.year > 9999 || my2k_wire_encode(&tm, b, sizeof b) < 0 || !my2k_aux_send(&tm)) return true;
        uint32_t crc = my2k_oled_crc(q + 3, 8);
        for (unsigned i = 0; i < 4; i++)
            expected[i] = (uint8_t)(crc >> (i * 8));
        if (++token == 0) ++token;
        state = 1;
        error = 0;
        if (!send_request(4, 0)) {
            state = 3;
            error = 5;
            return true;
        }
        r[2] = 0xaa;
        return true;
    }
    return true;
}

bool my2k_oled_main_busy(void) {
    return upload || state == 1 || waiting;
}
bool my2k_oled_main_ready(void) { return peer && loaded && state == 2; }

bool my2k_oled_export_owned(void) {
    return export_owned;
}
