/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_touch.h"
#include <stddef.h>
int my2k_touch_peak(const uint8_t raw[8]) {
    if (!raw)
        return -1;
    int     result  = -1;
    uint8_t maximum = MY2K_TOUCH_THRESHOLD;
    for (unsigned i = 0; i < 4; i++)
        if (raw[i] > maximum) {
            maximum = raw[i];
            result  = (int)i;
        }
    return result;
}
static void report(my2k_touch *t, uint16_t usage) {
    t->consumer = usage;
    if (t->emit)
        t->emit(t->context, MY2K_TOUCH_REPORT, usage);
}
void my2k_touch_init(my2k_touch *t, my2k_touch_emit_fn emit, void *context) {
    if (!t)
        return;
    t->state = MY2K_TOUCH_IDLE;
    for (unsigned i = 0; i < 8; i++)
        t->raw[i] = 0;
    t->contact_ms = 0;
    t->hold_sent  = false;
    t->consumer   = 0;
    t->emit       = emit;
    t->context    = context;
}
void my2k_touch_reset(my2k_touch *t) {
    if (!t)
        return;
    if (t->consumer)
        report(t, 0);
    my2k_touch_init(t, t->emit, t->context);
}
void my2k_touch_update(my2k_touch *t, const uint8_t raw[8], uint32_t now) {
    if (!t)
        return;
    bool changed = false;
    if (raw)
        for (unsigned i = 0; i < 4; i++)
            if (raw[i] != t->raw[i])
                changed = true;
    int old_peak = my2k_touch_peak(t->raw), peak = raw ? my2k_touch_peak(raw) : old_peak;
    int direction = old_peak >= 0 && peak >= 0 ? (peak > old_peak ? 1 : peak < old_peak ? -1 : 0) : 0;
    if (t->state == MY2K_TOUCH_IDLE) {
        if (changed) {
            if (peak >= 0) {
                t->hold_sent = false;
                t->state     = peak == 0   ? MY2K_TOUCH_LEFT_PENDING
                               : peak == 3 ? MY2K_TOUCH_RIGHT_PENDING
                                           : MY2K_TOUCH_MIDDLE;
            }
            t->contact_ms = now;
        }
    } else if (t->state == MY2K_TOUCH_MIDDLE) {
        if (changed) {
            if (peak < 0) {
                t->state = MY2K_TOUCH_IDLE;
                if (t->emit)
                    t->emit(t->context, MY2K_TOUCH_MUTE_TAP, MY2K_CONSUMER_MUTE);
            } else if (direction) {
                t->state = direction > 0 ? MY2K_TOUCH_UP : MY2K_TOUCH_DOWN;
                report(t, direction > 0 ? MY2K_CONSUMER_VOLUME_UP : MY2K_CONSUMER_VOLUME_DOWN);
            }
        }
    } else if (t->state == MY2K_TOUCH_DOWN || t->state == MY2K_TOUCH_UP) {
        if (changed) {
            if (peak < 0) {
                report(t, 0);
                t->state = MY2K_TOUCH_IDLE;
            } else if ((t->state == MY2K_TOUCH_DOWN && direction > 0) || (t->state == MY2K_TOUCH_UP && direction < 0)) {
                report(t, 0);
                t->state = t->state == MY2K_TOUCH_DOWN ? MY2K_TOUCH_UP : MY2K_TOUCH_DOWN;
                report(t, t->state == MY2K_TOUCH_UP ? MY2K_CONSUMER_VOLUME_UP : MY2K_CONSUMER_VOLUME_DOWN);
            }
        }
    } else if (t->state == MY2K_TOUCH_LEFT_PENDING || t->state == MY2K_TOUCH_RIGHT_PENDING) {
        bool left = t->state == MY2K_TOUCH_LEFT_PENDING;
        if (changed) {
            if (peak < 0) {
                if (t->hold_sent)
                    report(t, 0);
                t->state     = MY2K_TOUCH_IDLE;
                t->hold_sent = false;
            } else if ((left && direction > 0) || (!left && direction < 0)) {
                t->state = left ? MY2K_TOUCH_UP : MY2K_TOUCH_DOWN; /* Complete usage replaces prior held usage, as stock
                                                                      consumer report does. */
                report(t, left ? MY2K_CONSUMER_VOLUME_UP : MY2K_CONSUMER_VOLUME_DOWN);
            }
        } else if (!t->hold_sent && (uint32_t)(now - t->contact_ms) > MY2K_TOUCH_HOLD_MS) {
            t->hold_sent = true;
            report(t, left ? MY2K_CONSUMER_VOLUME_DOWN : MY2K_CONSUMER_VOLUME_UP);
        }
    }
    if (raw)
        for (unsigned i = 0; i < 8; i++)
            t->raw[i] = raw[i];
}
