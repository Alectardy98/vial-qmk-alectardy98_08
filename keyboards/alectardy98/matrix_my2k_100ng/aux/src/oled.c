/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_oled.h"
#include "aux_oled_store.h"
#include "aux_msc.h"
#include "aux_assets.h"
#include <string.h>
void aux_oled_init(aux_oled *o, aux_text *t) {
    memset(o, 0, sizeof *o);
    my2k_oled_defaults(o->config);
    o->dirty          = true;
    t->typing_enabled = true;
    memcpy(t->label, "MY2K", 5);
}
static void response(my2k_wire_message *r, unsigned op, unsigned token, unsigned offset, unsigned status) {
    *r = (my2k_wire_message){.type = MY2K_AR, .v = {op, token, offset, status}};
}
int aux_oled_request(aux_oled *o, aux_text *t, aux_panels *p, const my2k_wire_message *q, my2k_wire_message *r, uint32_t now) {
    unsigned op = q->v[0], token = q->v[1], off = q->v[2], n = q->v[3];
    response(r, op, token, off, 1);
    if (!token) return 1;
    if (op == 6) {
        if (off != 1 || o->ack_pending) return 1;
        if (!aux_assets_export_begin()) return 1;
        r->v[3] = 0;
        r->v[4] = 1;
        return 1;
    }
    if (aux_msc_export_active() && op != 0 && op != 5) return 1;
    if (op == 0) {
        response(r, 0, token, 0, 0);
        r->v[4] = 1;
        r->v[5] = 31 | (aux_oled_store_valid() ? 128 : 0);
        r->v[6] = 72;
        r->v[7] = 40;
        return 1;
    }
    if (op == 5) {
        if (off > 119) return 1;
        r->v[3] = 0;
        memcpy(r->v + 4, o->config + off * 4u, 4);
        return 1;
    }
    if (o->ack_pending) return 1;
    if (op == 4) {
        if (!p || !p->mono_ready) {
            r->v[3] = 2;
            return 1;
        }
        (void)aux_panel_request_mono_visible(p, true);
        o->ack_pending = true;
        o->ack_token   = token;
        o->ack_op      = 4;
        o->dirty       = true;
        return 0;
    }
    if (op == 1) {
        if (off != 120 || n != 1) return 1;
        o->next     = 0;
        o->token    = token;
        o->staging  = true;
        o->deadline = now + 3000;
        memset(o->staged, 0, MY2K_OLED_BYTES);
        r->v[3] = 0;
        return 1;
    }
    if (!o->staging || token != o->token || (int32_t)(now - o->deadline) >= 0) {
        o->staging = false;
        return 1;
    }
    if (op == 2) {
        if (n < 1 || n > 4 || off != o->next || off >= 120 || n != 4) return 1;
        memcpy(o->staged + off * 4u, q->v + 4, n);
        o->next++;
        o->deadline = now + 3000;
        r->v[3]     = 0;
        return 1;
    }
    if (op != 3 || off || n || o->next != 120 || !my2k_oled_valid(o->staged)) return 1;
    if (!p || !p->mono_ready) {
        r->v[3] = 2;
        return 1;
    }
    if (!aux_oled_store_save(o->staged)) {
        r->v[3]    = 3;
        o->staging = false;
        return 1;
    }
    (void)aux_panel_request_mono_visible(p, true);
    memcpy(o->config, o->staged, MY2K_OLED_BYTES);
    o->staging     = false;
    o->dirty       = true;
    o->ack_pending = true;
    o->ack_token   = token;
    o->ack_op      = 3;
    o->applies++;
    t->typing_enabled = (o->config[1] & 1) != 0;
    memset(t->label, 0, sizeof t->label);
    memcpy(t->label, o->config + 64, o->config[3]);
    if (!t->typing_enabled) t->current = 0;
    t->dirty = true;
    return 0;
}
static void pixel(uint8_t *f, unsigned x, unsigned y) {
    if (x < 72 && y < 40) f[(y / 8) * 72 + x] |= 1u << (y % 8);
}
static void happy(uint8_t *f, bool heart) {
    /* Original built-in icons, no asset file or filesystem access. */
    for (unsigned y = 0; y < 24; y++)
        for (unsigned x = 0; x < 24; x++) {
            bool on;
            if (heart) {
                int a = (int)x - 6, b = (int)x - 17, c = (int)y - 7;
                on = (y < 12 && (a * a + c * c < 42 || b * b + c * c < 42)) || (y >= 10 && y < 23 && x >= y - 10 && x <= 33 - y);
            } else {
                int a = (int)x - 11, b = (int)y - 11, d = a * a + b * b;
                on = (d > 95 && d < 135) || (y >= 6 && y <= 8 && (x == 7 || x == 15)) || (y >= 14 && y <= 17 && ((x == 6 || x == 16) || (y == 17 && x > 6 && x < 16)));
            }
            if (on) pixel(f, x + 24, y + 8);
        }
}
void aux_oled_service(aux_oled *o, aux_text *t, aux_datetime *clock, aux_panels *p, uint32_t now) {
    if (!p->mono_ready || !p->mono_enabled || p->mono_dirty || p->mono_blanked) return;
    if (o->clock_overlay && (int32_t)(now - o->clock_until) >= 0) o->clock_overlay = false;
    unsigned mode  = t->typing_enabled && t->current ? 1 : o->clock_overlay ? 2 : (o->config[1] & MY2K_OLED_CLOCK) ? 2 : (o->config[1] & 2) ? 3 : 4;
    unsigned phase = mode == 3 ? (now / 700u) & 1u : mode == 2 ? clock->second : 0;
    if (!o->dirty && !t->dirty && o->last_mode == mode && o->last_phase == phase && p->mono_overlay_enabled) return;
    if (mode == 1) {
        t->dirty = true;
        aux_text_service(t, p);
    } else if (mode == 2) {
        aux_text_clock_render(clock, p);
        t->dirty = false;
    } else {
        memset(p->mono_overlay, 0, sizeof p->mono_overlay);
        if (mode == 3)
            happy(p->mono_overlay, phase != 0);
        else if (o->config[1] & MY2K_OLED_BITMAP)
            memcpy(p->mono_overlay, o->config + MY2K_OLED_BITMAP_OFFSET, 360);
        else {
            unsigned x = 0, y = 0;
            for (unsigned i = 0; i < o->config[2]; i++) {
                unsigned c = o->config[4 + i];
                if (c == '\n') {
                    x = 0;
                    y++;
                    continue;
                }
                if (x == 11) {
                    x = 0;
                    y++;
                }
                aux_text_glyph(p->mono_overlay, 3 + x * 6, y * 8, c, 1);
                x++;
            }
        }
        p->mono_overlay_enabled = true;
        p->mono_dirty           = true;
        p->mono_page            = 0;
        t->dirty                = false;
    }
    o->last_mode  = mode;
    o->last_phase = phase;
    o->dirty      = false;
    o->frames++;
}
bool aux_oled_ack(aux_oled *o, const aux_text *t, const aux_panels *p, const aux_datetime *clock, my2k_wire_message *r) {
    if (!o->ack_pending || o->dirty || t->dirty || p->mono_dirty) return false;
    response(r, o->ack_op, o->ack_token, 0, p->mono_ready ? 0 : 2);
    if (o->ack_op == 3)
        memcpy(r->v + 4, o->config + MY2K_OLED_CRC_OFFSET, 4);
    else {
        (void)clock;
        uint32_t crc = o->clock_crc;
        for (unsigned i = 0; i < 4; i++)
            r->v[4 + i] = (uint8_t)(crc >> (i * 8));
    }
    o->ack_pending = false;
    return true;
}
