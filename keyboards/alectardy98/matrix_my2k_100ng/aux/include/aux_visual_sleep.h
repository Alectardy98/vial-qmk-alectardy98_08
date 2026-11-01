/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "aux_panels.h"
#include "my2k_wire.h"
bool aux_visual_sleep_request(const my2k_wire_message *, my2k_wire_message *);
bool aux_visual_sleep_service(aux_panels *, uint32_t, my2k_wire_message *);
