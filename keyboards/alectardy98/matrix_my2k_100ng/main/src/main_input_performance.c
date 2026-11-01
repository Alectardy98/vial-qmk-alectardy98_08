/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_input_performance.h"
#include <stddef.h>
#include <string.h>
#if MY2K_INPUT_PERFORMANCE_HOST
#    ifndef MY2K_INPUT_CYCLES_PER_US
#        define MY2K_INPUT_CYCLES_PER_US 168u
#    endif
#    define INPUT_LOCK() ((void)0)
#    define INPUT_UNLOCK() ((void)0)
#else
#    include "ch.h"
#    include "hal.h"
#    include "chibios_config.h"
#    define MY2K_INPUT_CYCLES_PER_US (REALTIME_COUNTER_CLOCK / 1000000u)
#    define INPUT_LOCK() chSysLock()
#    define INPUT_UNLOCK() chSysUnlock()
#endif
_Static_assert(MY2K_INPUT_CYCLES_PER_US > 0, "Realtime counter must resolve microseconds");
#if defined(MATRIX_ROWS) && defined(MATRIX_COLS)
_Static_assert(MATRIX_ROWS == 8 && MATRIX_COLS == 16, "MY2K input tracker geometry changed");
#endif
#define RAW_VALID 1u
#define RAW_PRESSED 2u
#define COOKED_VALID 4u
#define COOKED_PRESSED 8u
#define COOKED_RAW_VALID 16u
/* Keep the raw timestamp captured at cooking, rather than attributing a later
 * chatter edge to an earlier QMK event. Memory is fixed; no per-event queue. */
static uint32_t raw_at[128], cooked_at[128], cooked_raw_at[128];
static uint8_t  states[128];
static struct {
    uint32_t count, total_us, max_us, last_us, hist[8];
} metrics[MY2K_INPUT_METRICS];
static struct {
    bool     active, reported, raw_valid;
    uint32_t event, raw;
} current;
static uint32_t scans, raw_edges, cooked_edges, event_edges, queue_attempts, queue_failures, correlated, last_scan;
static bool     scan_seen;
static unsigned key_index(uint8_t row, uint8_t col) {
    return ((unsigned)row << 4) | col;
}
static bool key_valid(uint8_t row, uint8_t col) {
    return row < 8u && col < 16u;
}
static void add(unsigned id, uint32_t cycles) {
    if (id >= MY2K_INPUT_METRICS)
        return;
    uint32_t              us       = cycles / MY2K_INPUT_CYCLES_PER_US;
    static const uint16_t bounds[] = {125, 250, 500, 1000, 2000, 5000, 10000};
    unsigned              bin      = 0;
    while (bin < 7 && us > bounds[bin])
        bin++;
    metrics[id].count++;
    metrics[id].total_us += us;
    metrics[id].last_us = us;
    if (us > metrics[id].max_us)
        metrics[id].max_us = us;
    metrics[id].hist[bin]++;
}
__attribute__((noipa, externally_visible)) void my2k_input_reset(void) {
    memset(raw_at, 0, sizeof raw_at);
    memset(cooked_at, 0, sizeof cooked_at);
    memset(cooked_raw_at, 0, sizeof cooked_raw_at);
    memset(states, 0, sizeof states);
    memset(metrics, 0, sizeof metrics);
    memset(&current, 0, sizeof current);
    scans = raw_edges = cooked_edges = event_edges = queue_attempts = queue_failures = correlated = last_scan = 0;
    scan_seen                                                                                                 = false;
}
__attribute__((noipa, externally_visible)) void my2k_input_scan(uint32_t cycles) {
    scans++;
    if (scan_seen)
        add(MY2K_INPUT_SCAN_INTERVAL, cycles - last_scan);
    last_scan = cycles;
    scan_seen = true;
}
__attribute__((noipa, externally_visible)) void my2k_input_raw_edge(uint8_t row, uint8_t col, bool pressed,
                                                                    uint32_t cycles) {
    if (!key_valid(row, col))
        return;
    unsigned i = key_index(row, col);
    raw_at[i]  = cycles;
    states[i]  = (states[i] & ~RAW_PRESSED) | RAW_VALID | (pressed ? RAW_PRESSED : 0);
    raw_edges++;
}
__attribute__((noipa, externally_visible)) void my2k_input_cooked_edge(uint8_t row, uint8_t col, bool pressed,
                                                                       uint32_t cycles) {
    if (!key_valid(row, col))
        return;
    unsigned i   = key_index(row, col);
    uint8_t  f   = states[i];
    cooked_at[i] = cycles;
    f            = (f & ~(COOKED_PRESSED | COOKED_RAW_VALID)) | COOKED_VALID | (pressed ? COOKED_PRESSED : 0);
    if ((f & RAW_VALID) && !!(f & RAW_PRESSED) == pressed) {
        cooked_raw_at[i] = raw_at[i];
        f |= COOKED_RAW_VALID;
        add(MY2K_INPUT_RAW_TO_COOKED, cycles - raw_at[i]);
    }
    states[i] = f;
    cooked_edges++;
}
__attribute__((noipa, externally_visible)) void my2k_input_event_begin(uint8_t row, uint8_t col, bool pressed,
                                                                       bool keyboard_event, uint32_t cycles) {
    memset(&current, 0, sizeof current);
    event_edges++;
    if (!key_valid(row, col))
        return;
    unsigned i     = key_index(row, col);
    uint8_t  f     = states[i];
    bool     match = (f & COOKED_VALID) && !!(f & COOKED_PRESSED) == pressed;
    if (match)
        add(MY2K_INPUT_COOKED_TO_EVENT, cycles - cooked_at[i]);
    current.active    = keyboard_event;
    current.event     = cycles;
    current.raw_valid = keyboard_event && match && (f & COOKED_RAW_VALID);
    if (current.raw_valid)
        current.raw = cooked_raw_at[i];
    states[i] &= ~COOKED_VALID;
}
__attribute__((noipa, externally_visible)) void my2k_input_event_end(void) {
    memset(&current, 0, sizeof current);
}
__attribute__((noipa, externally_visible)) void my2k_input_report_queued(bool success, uint32_t begin_cycles,
                                                                         uint32_t end_cycles) {
    queue_attempts++;
    add(MY2K_INPUT_QUEUE_CALL, end_cycles - begin_cycles);
    if (!success) {
        queue_failures++;
        return;
    }
    if (!current.active || current.reported)
        return;
    current.reported = true;
    correlated++;
    add(MY2K_INPUT_EVENT_TO_QUEUE, end_cycles - current.event);
    if (current.raw_valid)
        add(MY2K_INPUT_RAW_TO_QUEUE, end_cycles - current.raw);
}
static void put32(uint8_t *p, uint32_t value) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(value >> (8 * i));
}
__attribute__((noipa, externally_visible)) bool my2k_input_status(uint8_t index, uint8_t report[32]) {
    if (!report || index > MY2K_INPUT_METRICS)
        return false;
    INPUT_LOCK();
    memset(report + 4, 0, 28);
    report[4] = 1;
    report[5] = index;
    if (!index) {
        const uint32_t values[] = {scans, raw_edges, cooked_edges, event_edges, queue_attempts, queue_failures};
        for (unsigned i = 0; i < 6; i++)
            put32(report + 8 + 4 * i, values[i]);
        report[6] = MY2K_INPUT_METRICS;
        report[7] = 8;
    } else {
        unsigned m = index - 1;
        report[6]  = (uint8_t)m;
        put32(report + 8, metrics[m].count);
        put32(report + 12, metrics[m].total_us);
        put32(report + 16, metrics[m].max_us);
        put32(report + 20, metrics[m].last_us);
        put32(report + 24, correlated);
        put32(report + 28, MY2K_INPUT_CYCLES_PER_US);
    }
    INPUT_UNLOCK();
    return true;
}
__attribute__((noipa, externally_visible)) bool my2k_input_histogram(uint8_t index, uint8_t report[32]) {
    /* One32-byte report has24payloadbytes after its envelope. Two bins cannot
     * fit with metadata; return six lower bins here and overflow summaries in
     * the final two fields of a second half (indices6..11). */
    if (!report || index >= MY2K_INPUT_METRICS * 2u)
        return false;
    unsigned m     = index % MY2K_INPUT_METRICS;
    bool     upper = index >= MY2K_INPUT_METRICS;
    INPUT_LOCK();
    memset(report + 4, 0, 28);
    report[4] = 1;
    report[5] = index;
    report[6] = (uint8_t)m;
    report[7] = upper;
    for (unsigned i = 0; i < (upper ? 2u : 6u); i++)
        put32(report + 8 + i * 4, metrics[m].hist[(upper ? 6u : 0u) + i]);
    INPUT_UNLOCK();
    return true;
}
