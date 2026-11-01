/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "my2k_wire.h"
bool aux_animation_request(const my2k_wire_message *, my2k_wire_message *);
#if MY2K_RELEASE1_PROFILE
/* Only the capability request may wait in the control queue
 * before peripheral initialization. The worker still handles it after startup;
 * accepting it early is not an ACK or a claim that any peripheral is ready. */
static inline bool aux_animation_startup_probe(const my2k_wire_message *q) {
    if (!q || q->type != MY2K_AC || q->v[0] != 8 || !q->v[1] || q->v[2] != 1 || (q->v[3] != 0 && q->v[3] != 5))
        return false;
    for (unsigned i = 4; i < 8; i++)
        if (q->v[i])
            return false;
    return true;
}
#endif
