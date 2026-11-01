/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_touch_engine.h"
#include <string.h>
static uint16_t get16(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}
bool my2k_touch_key_valid(uint16_t code) {
    /* Ordinary keyboard/modifier/media/mouse output, not Quantum actions,
     * layers, tap-hold, macros, resets or internal error/transparent usages. */
    if (!code)
        return true;
    if (code > 0x1fff)
        return false;
    unsigned base = code & 255;
    return (base >= 4 && base <= 0xc2) || (base >= 0xcd && base <= 0xe7);
}
uint32_t my2k_touch_crc(const uint8_t *p, unsigned n) {
    uint32_t c = ~0u;
    while (n--) {
        c ^= *p++;
        for (unsigned i = 0; i < 8; i++)
            c = (c >> 1) ^ ((0u - (c & 1u)) & 0xedb88320u);
    }
    return ~c;
}
void my2k_touch_defaults(uint8_t *p) {
    memset(p, 0, 16);
    p[0]       = 2;
    p[2]       = 50;
    p[3]       = 50;
    p[4]       = 0xaa;
    p[6]       = 0xa9;
    p[8]       = 0xa8;
    uint32_t c = my2k_touch_crc(p, 12);
    for (unsigned i = 0; i < 4; i++)
        p[12 + i] = (uint8_t)(c >> (8 * i));
}
bool my2k_touch_config_valid(const uint8_t *p) {
    if (!p || p[0] != 2 || p[1] > 1 || p[2] < 1 || p[2] > 100 || p[3] > 100 || p[10] || p[11])
        return false;
    for (unsigned i = 0; i < 3; i++)
        if (!my2k_touch_key_valid(get16(p + 4 + i * 2)))
            return false;
    uint32_t c = my2k_touch_crc(p, 12);
    for (unsigned i = 0; i < 4; i++)
        if (p[12 + i] != (uint8_t)(c >> (8 * i)))
            return false;
    return true;
}
bool my2k_touch_config_upgrade(uint8_t *p) {
    if (!p || p[0] != 1 || p[1] > 1 || p[2] < 1 || p[2] > 100 || p[3] || p[10] || p[11])
        return false;
    for (unsigned i = 0; i < 3; i++)
        if (!my2k_touch_key_valid(get16(p + 4 + i * 2)))
            return false;
    uint32_t c = my2k_touch_crc(p, 12);
    for (unsigned i = 0; i < 4; i++)
        if (p[12 + i] != (uint8_t)(c >> (8 * i)))
            return false;
    p[0] = 2;
    p[3] = 50;
    c    = my2k_touch_crc(p, 12);
    for (unsigned i = 0; i < 4; i++)
        p[12 + i] = (uint8_t)(c >> (8 * i));
    return true;
}
static void output(my2k_touch_engine *t, uint16_t code) {
    if (code == t->output)
        return;
    if (t->output && t->emit)
        t->emit(t->context, t->output, false);
    t->output = code;
    if (code && t->emit)
        t->emit(t->context, code, true);
}
static void enqueue(my2k_touch_engine *t, uint16_t code) {
    if (!code)
        return;
    if (t->count == MY2K_TOUCH_QUEUE) {
        t->dropped++;
        return;
    }
    t->queue[(t->head + t->count) % MY2K_TOUCH_QUEUE] = code;
    t->count++;
}
static void legacy(void *ctx, my2k_touch_event event, uint16_t usage) {
    my2k_touch_engine *t = ctx;
    if (event == MY2K_TOUCH_MUTE_TAP)
        enqueue(t, t->keys[2]);
    else {
        if (usage) {
            t->pulse_down = false;
            t->head = t->count = 0;
        }
        output(t, usage == MY2K_CONSUMER_VOLUME_DOWN ? t->keys[0] : usage == MY2K_CONSUMER_VOLUME_UP ? t->keys[1] : 0);
    }
}
void my2k_touch_engine_init(my2k_touch_engine *t, my2k_touch_key_fn emit, void *ctx) {
    memset(t, 0, sizeof *t);
    t->emit    = emit;
    t->context = ctx;
    my2k_touch_init(&t->hold, legacy, t);
    t->sensitivity = t->tap_jitter = 50;
    t->keys[0]                     = 0xaa;
    t->keys[1]                     = 0xa9;
    t->keys[2]                     = 0xa8;
}
void my2k_touch_engine_cancel(my2k_touch_engine *t) {
    my2k_touch_reset(&t->hold);
    output(t, 0);
    t->head = t->count = 0;
    t->contact = t->moved = t->pulse_down = false;
    t->candidate_direction                = 0;
}
void my2k_touch_engine_configure(my2k_touch_engine *t, const uint8_t *p) {
    if (!my2k_touch_config_valid(p))
        return;
    my2k_touch_engine_cancel(t);
    t->mode        = p[1];
    t->sensitivity = p[2];
    t->tap_jitter  = p[3];
    for (unsigned i = 0; i < 3; i++)
        t->keys[i] = get16(p + 4 + i * 2);
}
void my2k_touch_engine_sample(my2k_touch_engine *t, const uint8_t *raw, uint32_t now) {
    if (!raw)
        return;
    t->samples++;
    t->last_sample = now;
    memcpy(t->raw, raw, 4);
    unsigned threshold = 114u - t->sensitivity;
    if (!t->mode) {
        /* Keep the existing Hold recognizer/output timing. Filter only a
         * change of winning sensor, using the same0..768 centroid/radius as
         * Slider. Zero remains the exact legacy path. No additional RAM. */
        uint8_t  normalized[8] = {0};
        unsigned sum = 0, weighted = 0;
        for (unsigned i = 0; i < 4; i++)
            if (raw[i] > threshold) {
                unsigned w = raw[i] - threshold;
                sum += w;
                weighted += w * i * 256u;
                normalized[i] = (uint8_t)(65u + (w - 1u) * 190u / (254u - threshold));
            }
        if (!sum) {
            t->contact             = false;
            t->candidate_direction = 0;
            my2k_touch_update(&t->hold, normalized, now);
            return;
        }
        int position = (int)(weighted / sum);
        t->position  = (int16_t)position;
        if (!t->contact) {
            t->contact             = true;
            t->anchor              = (int16_t)position;
            t->candidate_direction = 0;
        } else if (t->tap_jitter && my2k_touch_peak(normalized) != my2k_touch_peak(t->hold.raw)) {
            int      delta    = position - t->anchor;
            unsigned distance = (unsigned)(delta < 0 ? -delta : delta);
            unsigned radius   = (unsigned)t->tap_jitter * 384u / 100u;
            if (distance <= radius) {
                t->candidate_direction = 0;
                my2k_touch_update(&t->hold, NULL, now);
                return;
            }
            int direction = delta > 0 ? 1 : -1;
            if (distance < 2u * radius) {
                if (t->candidate_direction != direction) {
                    t->candidate_direction = (int8_t)direction;
                    t->candidate_at        = now;
                    my2k_touch_update(&t->hold, NULL, now);
                    return;
                }
                if ((uint32_t)(now - t->candidate_at) < 20u) {
                    my2k_touch_update(&t->hold, NULL, now);
                    return;
                }
            }
            t->anchor              = (int16_t)position;
            t->candidate_direction = 0;
        } else
            t->candidate_direction = 0;
        my2k_touch_update(&t->hold, normalized, now);
        return;
    }
    unsigned sum = 0, weighted = 0;
    for (unsigned i = 0; i < 4; i++)
        if (raw[i] > threshold) {
            unsigned w = raw[i] - threshold;
            sum += w;
            weighted += w * i * 256u;
        }
    if (!sum) {
        if (t->contact && !t->moved && (uint32_t)(now - t->contact_at) <= 500u) {
            if (!t->candidate_direction) {
                enqueue(t, t->keys[2]);
                t->taps++;
            } else
                t->rejected++; /* Ambiguous short movement: no wrong action. */
        }
        t->contact             = false;
        t->candidate_direction = 0;
        return;
    }
    int position = (int)(weighted / sum);
    t->position  = (int16_t)position;
    if (!t->contact) {
        t->contact             = true;
        t->moved               = false;
        t->contact_at          = now;
        t->anchor              = (int16_t)position;
        t->candidate_direction = 0;
        return;
    }
    unsigned step = 256u - (unsigned)t->sensitivity * 256u / 100u;
    if (step < 32)
        step = 32;
    int delta = position - t->anchor;
    if (!t->moved && t->tap_jitter) {
        /* This radius is independent of swipe step/signal sensitivity. Keep
         * the original anchor so confirmation does not discard swipe travel. */
        unsigned distance = (unsigned)(delta < 0 ? -delta : delta);
        unsigned radius   = (unsigned)t->tap_jitter * 384u / 100u;
        if (distance <= radius) {
            t->candidate_direction = 0;
            return;
        }
        int      direction = delta > 0 ? 1 : -1;
        unsigned fast      = 2u * (radius > step ? radius : step);
        if (distance < fast) {
            if (t->candidate_direction != direction) {
                t->candidate_direction = (int8_t)direction;
                t->candidate_at        = now;
                return;
            }
            /* Only a new TC sample can confirm a borderline swipe. Timer
             * ticks never turn a cached landing/lift fluctuation into keys. */
            if (distance < step || (uint32_t)(now - t->candidate_at) < 20u)
                return;
        }
        t->candidate_direction = 0;
    }
    if (delta >= (int)step || delta <= -(int)step) {
        if (!t->moved)
            t->swipes++;
        t->moved      = true;
        int      sign = delta > 0 ? 1 : -1;
        unsigned n    = (unsigned)(delta * sign) / step;
        /* At most24 pulses from a sample; FIFO bounded16, no busy-wait. */
        for (unsigned i = 0; i < n; i++)
            enqueue(t, t->keys[sign > 0 ? 1 : 0]);
        t->anchor += (int16_t)(sign * (int)(n * step));
    }
}
void my2k_touch_engine_task(my2k_touch_engine *t, uint32_t now) {
    if (!t->mode)
        my2k_touch_update(&t->hold, NULL, now);
    if (t->pulse_down) {
        if ((uint32_t)(now - t->edge_at) < 8u)
            return;
        output(t, 0);
        t->pulse_down = false;
        t->edge_at    = now;
        return;
    }
    if (t->count && !t->output && (uint32_t)(now - t->edge_at) >= 8u) {
        uint16_t code = t->queue[t->head];
        t->head       = (t->head + 1) % MY2K_TOUCH_QUEUE;
        t->count--;
        output(t, code);
        t->pulse_down = true;
        t->edge_at    = now;
        t->pulses++;
    }
}
