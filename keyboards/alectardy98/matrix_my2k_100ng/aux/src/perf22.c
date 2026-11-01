/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_perf22.h"
#include <string.h>
#ifndef MY2K_PERF22_HOST
#    include "aux_platform.h"
static uint32_t started_ms;
#endif
#ifndef MY2K_ASSET_BATCH128
#    define MY2K_ASSET_BATCH128 0
#endif
#ifndef MY2K_D015_PIO_CONTROL
#    define MY2K_D015_PIO_CONTROL 0
#endif
static aux_perf22_frame live, last, snapshot;
static uint32_t         stamp, started, sequence;
static uint8_t          stack[12], depth, latch_epoch;
static bool             active, clock_ok, overflow;
#ifdef MY2K_PERF22_HOST
uint32_t        aux_perf22_host_clock;
static uint32_t ticks(void) {
    return aux_perf22_host_clock;
}
#else
#    define REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
static uint32_t ticks(void) {
    return REG(0xe0001004u);
}
#endif
void aux_perf22_init(void) {
#ifdef MY2K_PERF22_HOST
    clock_ok = true;
#else
    /* CMSIS CM7 DWT/LSR/LAR and CoreDebug DEMCR. Leave existing count intact.
     * This enables only the cycle counter, never watchpoints or debug halt. */
    REG(0xe000edfc) |= 1u << 24;
    if (REG(0xe0001fb4u) & 1u)
        REG(0xe0001fb0u) = 0xc5acce55u;
    if (!(REG(0xe0001000u) & (1u << 25)))
        REG(0xe0001000u) |= 1u;
    uint32_t a = ticks();
    __asm volatile("nop\nnop\nnop\nnop" ::: "memory");
    clock_ok = ticks() != a && (REG(0xe0001000u) & 1u);
#endif
    active      = false;
    depth       = 0;
    sequence    = 0;
    latch_epoch = 0;
    memset(&last, 0, sizeof last);
    memset(&snapshot, 0, sizeof snapshot);
}
static void charge(void) {
    uint32_t now = ticks();
    if (active)
        live.cycles[stack[depth]] += now - stamp;
    stamp = now;
}
bool aux_perf22_enter(unsigned phase) {
    if (!active || phase >= PERF22_PHASES)
        return false;
    if (depth + 1u >= sizeof stack) {
        overflow = true;
        return false;
    }
    charge();
    stack[++depth] = (uint8_t)phase;
    return true;
}
void aux_perf22_leave(bool entered) {
    if (entered && active) {
        charge();
        if (depth)
            depth--;
        else
            overflow = true;
    }
}
void aux_perf22_count(unsigned c) {
    if (active && c < PERF22_COUNTS && live.count[c] != 65535u)
        live.count[c]++;
}
void aux_perf22_begin(uint16_t interval) {
    memset(&live, 0, sizeof live);
    depth            = 0;
    stack[0]         = PERF22_GAP;
    overflow         = false;
    live.interval_ms = interval;
    live.variant     = MY2K_D015_PIO_CONTROL ? 0 : MY2K_ASSET_BATCH128 ? 2 : 1;
#ifndef MY2K_PERF22_HOST
    started_ms = aux_millis();
#endif
    stamp = started = ticks();
    active          = true;
}
void aux_perf22_end(bool success) {
    if (!active)
        return;
    charge();
    live.elapsed = stamp - started;
    live.valid   = clock_ok && success && !overflow && !depth;
#ifndef MY2K_PERF22_HOST
    /*216MHz CYCCNT wraps in19.88s. Never publish a long frame as a short one. */
    if ((uint32_t)(aux_millis() - started_ms) >= 19000u)
        live.valid = 0;
#endif
    live.sequence = ++sequence;
    last          = live;
    active        = false;
}
static void put32(uint8_t *b, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        b[i] = (uint8_t)(v >> (8 * i));
}
static void put16(uint8_t *b, uint16_t v) {
    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
}
bool aux_perf22_page(uint8_t page, uint8_t b[8]) {
    if (page < 10 || page > 19 || !b)
        return false;
    memset(b, 0, 8);
    b[0] = page;
    /* DQ10 latches a coherent *completed* frame in foreground. DQ11..19 read
     * the same RAM snapshot until the next DQ10; pages carry independent ages. */
    if (page == 10) {
        snapshot    = last;
        latch_epoch = (latch_epoch + 1u) & 63u;
        b[1]        = 22;
        b[2]        = snapshot.variant | (latch_epoch << 2);
        b[3]        = snapshot.valid;
        put32(b + 4, snapshot.sequence);
        return true;
    }
    b[7] = latch_epoch;
    if (page == 11) {
        put32(b + 1, snapshot.elapsed);
        put16(b + 5, snapshot.interval_ms);
        return true;
    }
    static const uint8_t phases[] = {PERF22_FILE, PERF22_SD,        PERF22_DECODE, PERF22_PANEL,
                                     PERF22_SPI,  PERF22_SUSPENDED, PERF22_GAP};
    static const uint8_t counts[] = {PERF22_FILE_CALL, PERF22_SD_CALL,    PERF22_PIXEL_CALL, PERF22_SEEK_CALL,
                                     PERF22_SEND_CALL, PERF22_YIELD_CALL, PERF22_COUNTS};
    if (page <= 18) {
        unsigned n = page - 12;
        put32(b + 1, snapshot.cycles[phases[n]]);
        put16(b + 5, counts[n] < PERF22_COUNTS ? snapshot.count[counts[n]] : 0);
        return true;
    }
#ifndef MY2K_PERF22_HOST
    /* Read-only system clock/cache observations, not a cache or clock change. */
    put32(b + 1, REG(0x40023808u));
    b[5] = (uint8_t)(REG(0xe000ed14u) >> 16);
    b[6] = (uint8_t)(REG(0xe0001000u) & 1u);
#else
    b[6] = clock_ok;
#endif
    return true;
}
