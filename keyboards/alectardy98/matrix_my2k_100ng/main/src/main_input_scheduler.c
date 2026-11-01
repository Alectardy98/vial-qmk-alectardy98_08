/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_input_scheduler.h"
#include "main_input_performance.h"
#include "main_input_io.h"
#include <string.h>
#if MY2K_INPUT_SCHEDULER_HOST
#    ifndef MY2K_INPUT_CYCLES_PER_US
#        define MY2K_INPUT_CYCLES_PER_US 168u
#    endif
#    define SCHED_LOCK() ((void)0)
#    define SCHED_UNLOCK() ((void)0)
#else
#    include "hal.h"
#    include "keyboard.h"
#    include "main_perf.h"
#    include "chibios_config.h"
#    define MY2K_INPUT_CYCLES_PER_US (REALTIME_COUNTER_CLOCK / 1000000u)
#    define SCHED_LOCK() chSysLock()
#    define SCHED_UNLOCK() chSysUnlock()
static uint32_t my2k_input_scheduler_clock(void) {
    return my2k_perf_clock();
}
static bool my2k_input_scheduler_context_ready(void) {
    return !__get_IPSR() && !__get_PRIMASK() && !__get_BASEPRI();
}
__attribute__((noipa)) static void my2k_input_scheduler_keyboard_task(void) {
    keyboard_task();
}
#endif
_Static_assert(MY2K_INPUT_CYCLES_PER_US > 0, "Realtime clock must resolve microseconds");
/* USB remains125us. A100us internal budget leaves25us for task-entry,
 * RTOS wakeup and report work; faster scans are not faster USB polling. */
#if MY2K_ACTIVE_DEV_PROFILE
#    define INPUT_PERIOD_US 100u
#else
#    define INPUT_PERIOD_US 125u
#endif
#define PERIOD_CYCLES (INPUT_PERIOD_US * MY2K_INPUT_CYCLES_PER_US)
static bool     window, enabled, busy, scan_seen;
static uint32_t last_scan, scans, calls, context_refusals, reentrant_refusals, unsafe_refusals;
static uint32_t last_us, max_us, total_us, max_late_us, no_scan_callbacks;
__attribute__((noipa, externally_visible)) void my2k_input_scheduler_reset(void) {
    window = enabled = busy = scan_seen = false;
    last_scan = scans = calls = context_refusals = reentrant_refusals = unsafe_refusals = 0;
    last_us = max_us = total_us = max_late_us = no_scan_callbacks = 0;
}
__attribute__((noipa, externally_visible)) void my2k_input_scheduler_scan(uint32_t cycles) {
    last_scan = cycles;
    scan_seen = true;
    scans++;
}
__attribute__((noipa, externally_visible)) void my2k_input_scheduler_enter(bool high, bool ready, bool storage) {
    if (busy)
        return;
    window  = true;
    enabled = high && ready && !storage;
    if (enabled)
        my2k_input_io_init();
}
__attribute__((noipa, externally_visible)) void my2k_input_scheduler_leave(void) {
    if (!busy) {
        window  = false;
        enabled = false;
    }
}
__attribute__((noipa, externally_visible)) bool my2k_input_scheduler_checkpoint(bool safe_point) {
    if (busy) {
        reentrant_refusals++;
        return false;
    }
    if (!window || !enabled || !scan_seen)
        return false;
    if (!safe_point) {
        unsafe_refusals++;
        return false;
    }
    if (!my2k_input_scheduler_context_ready()) {
        context_refusals++;
        return false;
    }
    uint32_t started = my2k_input_scheduler_clock(), elapsed = started - last_scan;
    if (elapsed < PERIOD_CYCLES)
        return false;
    uint32_t late = (elapsed - PERIOD_CYCLES) / MY2K_INPUT_CYCLES_PER_US;
    if (late > max_late_us)
        max_late_us = late;
    uint32_t before_scans = scans;
    busy                  = true;
    /* Clear any action context whose later QMK processor skipped its post hook.
     * This is an observation reset, never a key or USB report operation. */
    my2k_input_event_end();
    my2k_input_scheduler_keyboard_task();
    my2k_input_event_end();
    uint32_t finished = my2k_input_scheduler_clock();
    busy              = false;
    calls++;
    last_us = (finished - started) / MY2K_INPUT_CYCLES_PER_US;
    total_us += last_us;
    if (last_us > max_us)
        max_us = last_us;
    if (scans == before_scans) {
        /* Do not spin on a callback that did not execute a physical scan. */
        no_scan_callbacks++;
        last_scan = finished;
    } else if (finished - started >= PERIOD_CYCLES) {
        /* A long QMK action (for example a user macro) is not a request for
         * overdue catchup bursts. Actual scan metrics remain independent. */
        last_scan = finished;
    }
    return true;
}
bool my2k_input_scheduler_can_io(void) {
    return window && enabled && scan_seen && !busy;
}
unsigned my2k_input_scheduler_wait_us(void) {
    if (!my2k_input_scheduler_can_io())
        return 125u;
    uint32_t elapsed = my2k_input_scheduler_clock() - last_scan;
    return elapsed >= PERIOD_CYCLES
               ? 0u
               : (PERIOD_CYCLES - elapsed + MY2K_INPUT_CYCLES_PER_US - 1u) / MY2K_INPUT_CYCLES_PER_US;
}
static void put32(uint8_t *p, uint32_t value) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(value >> (8 * i));
}
__attribute__((noipa, externally_visible)) bool my2k_input_scheduler_status(uint8_t index, uint8_t report[32]) {
    if (!report || index > 2)
        return false;
#if MY2K_ACTIVE_DEV_PROFILE && !MY2K_INPUT_SCHEDULER_HOST
    if (index == 2)
        return my2k_input_io_status(report);
#else
    if (index == 2)
        return false;
#endif
    SCHED_LOCK();
    memset(report + 4, 0, 28);
    report[4]                  = 1;
    report[5]                  = index;
    report[6]                  = (window ? 1u : 0u) | (enabled ? 2u : 0u) | (busy ? 4u : 0u) | (scan_seen ? 8u : 0u);
    const uint32_t  summary[]  = {calls, last_us, max_us, total_us, max_late_us, scans};
    const uint32_t  refusals[] = {context_refusals,        reentrant_refusals, unsafe_refusals, no_scan_callbacks, 125,
                                  MY2K_INPUT_CYCLES_PER_US};
    const uint32_t *p          = index ? refusals : summary;
    for (unsigned i = 0; i < 6; i++)
        put32(report + 8 + i * 4, p[i]);
    SCHED_UNLOCK();
    return true;
}
