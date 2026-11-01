/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "aux_text.h"
#include "aux_clock.h"
#include "my2k_oled_config.h"
typedef struct {
    uint8_t  config[MY2K_OLED_BYTES], staged[MY2K_OLED_BYTES], next, token, last_mode, last_phase;
    bool     staging, dirty, ack_pending, clock_overlay;
    uint32_t deadline, clock_until, clock_crc, applies, frames;
    uint8_t  ack_token, ack_op;
} aux_oled;
void aux_oled_init(aux_oled *, aux_text *);
/* Returns1:reply now,0:commit awaiting completed OLED transfer,-1:invalid. */
int  aux_oled_request(aux_oled *, aux_text *, aux_panels *, const my2k_wire_message *, my2k_wire_message *, uint32_t);
void aux_oled_service(aux_oled *, aux_text *, aux_datetime *, aux_panels *, uint32_t);
bool aux_oled_ack(aux_oled *, const aux_text *, const aux_panels *, const aux_datetime *, my2k_wire_message *);
