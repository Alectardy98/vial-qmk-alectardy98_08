/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_visual_sleep.h"
#include "aux_platform.h"
#include <string.h>
static my2k_wire_message pending;
static uint32_t          requested;
bool                     aux_visual_sleep_request(const my2k_wire_message *q, my2k_wire_message *r) {
    if (!q || !r || q->type != MY2K_AC || (q->v[0] != 13 && q->v[0] != 14))
        return false;
    *r = (my2k_wire_message){.type = MY2K_AR};
    memcpy(r->v, q->v, 3);
    r->v[3] = 1;
    if (!q->v[1] || q->v[3] != 1 || q->v[2] > (q->v[0] == 13 ? 0u : 1u))
        return true;
    for (unsigned i = 4; i < 8; i++)
        if (q->v[i])
            return true;
    if (q->v[0] == 13) {
        r->v[3] = 0;
        memcpy(r->v + 4, "SLP1", 4);
        return true;
    }
    if (pending.type) {
        r->v[3] = 2;
        return true;
    }
    pending   = *q;
    requested = aux_millis();
    r->type   = 0;
    return true;
}
bool aux_visual_sleep_service(aux_panels *p, uint32_t now, my2k_wire_message *r) {
    if (!pending.type)
        return false;
    if (p->color_streaming || aux_panel_color_busy(p)) {
        if ((uint32_t)(now - requested) < 1000u)
            return false;
        *r = (my2k_wire_message){.type = MY2K_AR};
        memcpy(r->v, pending.v, 3);
        r->v[3]      = 2;
        pending.type = 0;
        return true;
    }
    *r = (my2k_wire_message){.type = MY2K_AR};
    memcpy(r->v, pending.v, 3);
    bool okay = aux_panels_visual_sleep(p, pending.v[2] != 0);
    r->v[3]   = okay ? 0 : 3;
    r->v[4]   = p->color_blanked && p->mono_blanked;
    r->v[5]   = (!p->color_blanked ? 0 : 1) | (!p->mono_blanked ? 0 : 2);
    if (!pending.v[2] && okay)
        r->v[5] = 3; /* Both awake confirmed. */
    pending.type = 0;
    return true;
}
