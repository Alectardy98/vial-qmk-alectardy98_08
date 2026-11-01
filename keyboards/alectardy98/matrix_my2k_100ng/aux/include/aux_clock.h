/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "my2k_wire.h"
typedef struct {
    uint16_t year;
    uint8_t  month, day, weekday, hour, minute, second;
    uint32_t anchor_ms;
    bool     valid;
} aux_datetime;
bool aux_datetime_set(aux_datetime *, const my2k_wire_message *, uint32_t now);
bool aux_datetime_step(aux_datetime *, uint32_t now);
