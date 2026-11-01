/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "my2k_wire.h"
void my2k_sleep_init(uint32_t now);
void my2k_sleep_activity(uint32_t now);
void my2k_sleep_service(uint32_t now, bool inhibited);
void my2k_sleep_receive(const my2k_wire_message *);
bool my2k_sleep_blank(void);
bool my2k_sleep_hid(uint8_t *, unsigned, bool);
