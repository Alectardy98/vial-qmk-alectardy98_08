/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "quantum.h"
#include "my2k_wire.h"
void my2k_games_service(uint32_t);
void my2k_games_receive(const my2k_wire_message *);
bool my2k_games_key(uint16_t, keyrecord_t *);
bool my2k_games_hid(uint8_t *, unsigned);

bool my2k_games_running(void);
