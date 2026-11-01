/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "my2k_wire.h"
void my2k_animation_init(void);
void my2k_animation_service(uint32_t now);
void my2k_animation_receive(const my2k_wire_message *);
bool my2k_animation_hid(uint8_t *, unsigned, bool);
void my2k_animation_key(uint8_t action);

void my2k_animation_return(void);
