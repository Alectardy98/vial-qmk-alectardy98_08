/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "aux_panels.h"
#include "my2k_wire.h"
bool aux_nes_request(const my2k_wire_message *,my2k_wire_message *);
void aux_nes_service(aux_panels *,uint32_t);
bool aux_nes_active(void);
bool aux_nes_runnable(uint32_t);
