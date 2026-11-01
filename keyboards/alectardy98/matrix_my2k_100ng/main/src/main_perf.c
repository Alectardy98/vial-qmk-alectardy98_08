/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_perf.h"
#ifndef MY2K_PERF_HOST
#    include "ch.h"
#    include "hal.h"
#    include "chibios_config.h"
#    define MY2K_PERF_MHZ (REALTIME_COUNTER_CLOCK / 1000000u)
__attribute__((noipa, externally_visible)) uint32_t my2k_perf_clock(void) {
    return chSysGetRealtimeCounterX();
}
#else
#    define MY2K_PERF_MHZ 168u
#endif
static my2k_perf_metric metrics[MY2K_PERF_COUNT];
void                    my2k_perf_record(unsigned id, uint32_t start) {
    if (id >= MY2K_PERF_COUNT)
        return;
    uint32_t          elapsed = (uint32_t)(my2k_perf_clock() - start) / MY2K_PERF_MHZ;
    my2k_perf_metric *m       = &metrics[id];
    m->count++;
    m->total_us += elapsed;
    m->last_us = elapsed;
    if (elapsed > m->max_us)
        m->max_us = elapsed;
    if (m->seen) {
        uint32_t interval   = (uint32_t)(start - m->last_start) / MY2K_PERF_MHZ;
        m->last_interval_us = interval;
        if (interval > m->max_interval_us)
            m->max_interval_us = interval;
    }
    m->seen       = 1;
    m->last_start = start;
}
my2k_perf_metric my2k_perf_get(unsigned id) {
    return id < MY2K_PERF_COUNT ? metrics[id] : (my2k_perf_metric){0};
}
