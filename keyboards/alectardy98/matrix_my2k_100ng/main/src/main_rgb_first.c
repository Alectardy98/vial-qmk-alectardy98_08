/* SPDX-License-Identifier: GPL-2.0-or-later */
/* New Dev21 first-failure recorder, authored from verified Dev19 interfaces.
 * No recovery; defined function shutdown register only; no PWM/frame reads. */
#include "main_rgb_first.h"
#include "main_rgb_observer.h"
#include "main_rgb_bus.h"
#include "main_perf.h"
#include <string.h>
#ifndef MY2K_FIRST_HOST
#    include "hal.h"
#    include "timer.h"
#else
uint32_t timer_read32(void);
#endif
/* Reuse3KiB of legacy RGB trace RAM for the input DMA-wait worker.
 * Keep fault+48post-events and independent32-key history. Capacity is already
 * negotiated in GET45; immutable older releases retain their192events. */
#if MY2K_ACTIVE_DEV_PROFILE
#    define N 144u
#else
#    define N 192u
#endif
#define POST 48u
/* Host format is explicitly little-endian; sizeof checked against ARM too. */
typedef struct {
    uint32_t seq, ms, cycles, detail;
    uint16_t count;
    uint8_t  kind, address, bank, reg, status, fault, data[8];
} event;
_Static_assert(sizeof(event) == 32, "First trace ABI");
static event    ring[N], view[N], key_ring[32], key_view[32];
static unsigned key_next, key_used, key_view_used;
static unsigned next, used, view_used, post;
static uint32_t sequence, overwritten, fault_sequence, fault_ms, keys, last_service, last_poll;
static uint8_t  reason, chip, unsafe, good_mask, zero_mask, last_value[3], view_reason;
static uint32_t zero_at[3];
static bool     frozen;
static uint32_t view_sequence, view_fault_sequence, view_fault_ms, view_overwritten, view_keys;
#ifndef MY2K_FIRST_HOST
static uint32_t observed[12];
static bool     have_observed;
static void     sample_registers(void) {
    /* Never read I2C SR1/SR2: their sequence can clear protocol conditions. */
    static const uint32_t addresses[] = {0x40020400, 0x40020404, 0x4002040c, 0x40020420, 0x40020410, 0x40023820,
                                         0x40023840, 0x40005400, 0x40005404, 0x4000541c, 0x40005420, 0x40020414};
    for (unsigned i = 0; i < 12; i++) {
        uint32_t v = *(volatile uint32_t *)(uintptr_t)addresses[i];
        if (i == 4 || i == 11)
            v &= (1u << 6) | (1u << 7); /* bus levels/ODR only */
        if (!have_observed || observed[i] != v) {
            uint8_t bytes[8];
            memcpy(bytes, &observed[i], 4);
            memcpy(bytes + 4, &v, 4);
            my2k_first_event(FIRST_REGISTERS, 0, 0, (uint8_t)i, 8, 0, 0, addresses[i], bytes);
            observed[i] = v;
        }
    }
    have_observed = true;
}
#endif
void my2k_first_rearm(void) {
    /* Explicit operator action, RAM only. Save the old frozen download first. */
    next = used = view_used = post = key_next = key_used = key_view_used = 0;
    sequence = overwritten = fault_sequence = fault_ms = keys = 0;
    reason = good_mask = zero_mask = view_reason = 0;
    frozen                                       = false;
    memset(last_value, 0, sizeof last_value);
    /* Do not alter bank safety, physical bus, settings, last_poll or controllers. */
}
void my2k_first_event(uint8_t type, uint8_t address, uint8_t bank, uint8_t reg, uint16_t count, uint8_t status,
                      uint8_t fault, uint32_t detail, const uint8_t *data) {
    if (frozen)
        return;
    event *e = &ring[next];
    memset(e, 0, sizeof *e);
    e->seq     = ++sequence;
    e->ms      = timer_read32();
    e->cycles  = my2k_perf_clock();
    e->detail  = detail;
    e->count   = count;
    e->kind    = type;
    e->address = address;
    e->bank    = bank;
    e->reg     = reg;
    e->status  = status;
    e->fault   = fault;
    if (data) {
        unsigned n = count > 8 ? 8 : count;
        memcpy(e->data, data, n);
    }
    next = (next + 1u) % N;
    if (used < N)
        used++;
    else
        overwritten++;
    if (reason && ++post >= POST)
        frozen = true;
}
void my2k_first_trigger(uint8_t why, uint8_t address, uint8_t reg) {
    if (reason || frozen)
        return;
    /* First fault event is retained even if the ring had just wrapped. */
    my2k_first_event(FIRST_FAULT, address, 0xff, reg, 0, 0, why, 0, 0);
    reason         = why;
    fault_sequence = sequence;
    fault_ms       = timer_read32();
    post           = 0;
}
void my2k_first_key(uint8_t row, uint8_t col, uint16_t code, bool pressed, uint32_t layers, uint8_t mods, uint8_t weak,
                    uint8_t default_layer) {
    if (frozen)
        return;
    uint8_t d[] = {row, col, (uint8_t)code, (uint8_t)(code >> 8), pressed, mods, weak, default_layer};
    keys++;
#ifndef MY2K_FIRST_HOST
    uint8_t context = (uint8_t)__get_IPSR(), mask = (uint8_t)__get_BASEPRI();
#else
    uint8_t context = 0, mask = 0;
#endif
    my2k_first_event(FIRST_KEY, 0, 0, 0, 8, context, mask, layers, d);
    key_ring[key_next] = ring[(next + N - 1) % N];
    key_next           = (key_next + 1) % 32;
    if (key_used < 32)
        key_used++;
}
void my2k_first_health(uint8_t addr, uint8_t value, bool valid) {
    if (addr != 0x74 && addr != 0x77 && addr != 0x75)
        return;
    unsigned i   = addr == 0x74 ? 0 : addr == 0x77 ? 1 : 2;
    uint8_t  bit = 1u << i;
    uint8_t  d[] = {value, last_value[i], good_mask, zero_mask};
    my2k_first_event(FIRST_SHUTDOWN, addr, 11, 10, 4, valid ? 0 : 1, 0, zero_at[i], d);
    if (!valid) {
        zero_mask &= ~bit;
        return;
    }
    last_value[i] = value;
    if (value & 1) {
        good_mask |= bit;
        zero_mask &= ~bit;
        return;
    }
    /* A zero is a suspected fault only after a successful awake sample. It is
     * not called corruption or proof of a writer. Subsequent samples confirm. */
    if (good_mask & bit) {
        if (!(zero_mask & bit)) {
            zero_at[i] = timer_read32();
            zero_mask |= bit;
            my2k_first_trigger(2, addr, 10);
        } else if ((uint32_t)(timer_read32() - zero_at[i]) >= 10u)
            my2k_first_event(FIRST_SHUTDOWN, addr, 11, 10, 1, 0, 2, zero_at[i], &value);
    }
}
void my2k_first_service(uint32_t now) {
    if (frozen)
        return;
    if (reason && (uint32_t)(now - fault_ms) >= 250u) {
        frozen = true;
        return;
    }
    if (last_service && (uint32_t)(now - last_service) > 5u)
        my2k_first_event(FIRST_GAP, 0, 0, 0, 0, 0, 0, now - last_service, 0);
    last_service = now;
    if (now < 1000u || (uint32_t)(now - last_poll) < 50u)
        return;
    last_poll = now;
#ifndef MY2K_FIRST_HOST
    sample_registers();
#endif
    static const uint8_t addresses[] = {0x74, 0x77, 0x75};
    unsigned             i           = chip;
    chip                             = (chip + 1u) % 3u;
    uint8_t addr = addresses[i], bank = 11, value = 0;
    bool    ok = my2k_rgb_bus_write(addr << 1, 0xfd, &bank, 1, 5) == 0;
    if (ok)
        ok = my2k_rgb_bus_read(addr << 1, 10, &value, 1, 5) == 0;
    bank          = 0;
    bool restored = my2k_rgb_bus_write(addr << 1, 0xfd, &bank, 1, 5) == 0;
    if (restored)
        unsafe &= ~(1u << (i + 2));
    else
        unsafe |= 1u << (i + 2);
    my2k_first_health(addr, value, ok && restored);
}
/* Replace Dev19's finite post-key observer only in the new target. */
bool my2k_rgb_observer_bank_safe(unsigned controller) {
    return controller < 6 && !(unsafe & (1u << controller));
}
void my2k_rgb_observer_key(uint8_t row, uint8_t col, uint16_t code, bool down) {
    (void)row;
    (void)col;
    (void)code;
    (void)down;
}
void my2k_rgb_observer_service(const my2k_rgb_state *state, uint32_t now) {
    (void)state;
    my2k_first_service(now);
}
bool my2k_rgb_observer_page(uint8_t page, uint8_t index, uint8_t out[32]) {
    (void)page;
    (void)index;
    (void)out;
    return false;
}
static void put(uint8_t *p, uint32_t n) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(n >> (i * 8));
}
bool my2k_first_page(uint8_t page, uint8_t index, uint8_t out[32]) {
    if (!out || page < 45 || page > 50)
        return false;
    if ((page == 45 || page == 48) ? index != 0 : index >= (page < 48 ? view_used : key_view_used))
        return false;
    memset(out + 4, 0, 28);
    out[4] = 21;
    if (page == 45) {
        /* Stable RAM-only snapshot. Query never arms, clears or samples hardware. */
        for (unsigned i = 0; i < used; i++)
            view[i] = ring[(next + N - used + i) % N];
        for (unsigned i = 0; i < key_used; i++)
            key_view[i] = key_ring[(key_next + 32 - key_used + i) % 32];
        key_view_used       = key_used;
        view_used           = used;
        view_reason         = reason;
        view_sequence       = sequence;
        view_fault_sequence = fault_sequence;
        view_fault_ms       = fault_ms;
        view_overwritten    = overwritten;
        view_keys           = keys;
        out[5]              = view_used;
        out[6]              = N;
        out[7]              = frozen ? 2 : reason ? 1 : 0;
        put(out + 8, view_sequence);
        put(out + 12, view_fault_sequence);
        put(out + 16, view_fault_ms);
        put(out + 20, view_overwritten);
        put(out + 24, view_keys);
        out[28] = view_reason;
        out[29] = good_mask;
        out[30] = zero_mask;
        out[31] = unsafe;
    } else if (page == 48) {
        out[5] = key_view_used;
        put(out + 8, view_sequence);
    } else {
        out[5] = index;
        out[6] = view_reason;
        put(out + 8, view_sequence);
        memcpy(out + 16,
               (const uint8_t *)(page < 48 ? &view[index] : &key_view[index]) + ((page == 47 || page == 50) ? 16 : 0),
               16);
    }
    return true;
}
