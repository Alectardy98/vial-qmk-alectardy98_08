/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Task-context instrumentation only. All timestamps are the existing realtime
 * cycle counter; no new timer/register or hardware operation is performed. */
enum {
    MY2K_INPUT_RAW_TO_COOKED,
    MY2K_INPUT_COOKED_TO_EVENT,
    MY2K_INPUT_EVENT_TO_QUEUE,
    MY2K_INPUT_RAW_TO_QUEUE,
    MY2K_INPUT_QUEUE_CALL,
    MY2K_INPUT_SCAN_INTERVAL,
    MY2K_INPUT_METRICS
};
void my2k_input_reset(void);
void my2k_input_scan(uint32_t cycles);
void my2k_input_raw_edge(uint8_t row,uint8_t col,bool pressed,uint32_t cycles);
void my2k_input_cooked_edge(uint8_t row,uint8_t col,bool pressed,uint32_t cycles);
/* Begin/end bracket QMK's processing of ONE physical event. "keyboard_event"
 * excludes layer/custom/no-op/consumer/mouse events. Post-action end discards
 * unmatched events, so a suppressed/duplicate report cannot taint a later one. */
void my2k_input_event_begin(uint8_t row,uint8_t col,bool pressed,bool keyboard_event,uint32_t cycles);
void my2k_input_event_end(void);
/* Only keyboard/NKRO endpoint calls. Successful queueing is not host receipt.
 * Call even on failure; queue-call timing does not imply wire cadence. */
void my2k_input_report_queued(bool success,uint32_t begin_cycles,uint32_t end_cycles);
/* GET28 index0 summary, index1..6 metrics; GET29 indices0..5 bins0..5, indices6..11 bins6..7 for metric(index%6).
 * Existing HID envelope bytes0..3 are untouched. Index failure writes nothing. */
bool my2k_input_status(uint8_t index,uint8_t report[32]);
bool my2k_input_histogram(uint8_t index,uint8_t report[32]);
