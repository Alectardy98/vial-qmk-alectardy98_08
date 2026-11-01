/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#define AUX_TIMELINE_FRAMES 512u
typedef struct {
    uint32_t end[AUX_TIMELINE_FRAMES], duration, epoch;
    uint16_t count;
    bool     started;
} aux_animation_clock;
static inline void aux_animation_clock_clear(aux_animation_clock *c) {
    c->duration = 0;
    c->count    = 0;
    c->started  = false;
}
static inline bool aux_animation_clock_add(aux_animation_clock *c, uint16_t delay) {
    if (c->count >= AUX_TIMELINE_FRAMES)
        return false;
    /* Match the existing preview's zero-delay minimum; never spin on a zero loop. */
    uint32_t ms = delay ? delay : 10u;
    c->duration += ms;
    c->end[c->count++] = c->duration;
    return true;
}
static inline uint16_t aux_animation_clock_frame(aux_animation_clock *c, uint32_t now) {
    if (!c->count || !c->duration)
        return UINT16_MAX;
    if (!c->started) {
        c->epoch   = now;
        c->started = true;
    }
    uint32_t at = (uint32_t)(now - c->epoch) % c->duration;
    unsigned lo = 0, hi = c->count;
    while (lo < hi) {
        unsigned mid = lo + (hi - lo) / 2u;
        if (at < c->end[mid])
            hi = mid;
        else
            lo = mid + 1u;
    }
    return (uint16_t)lo;
}
