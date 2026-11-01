/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "aux_panels.h"
#include <stdint.h>
#include <stdbool.h>
unsigned aux_gbc_start(aux_panels *);
unsigned aux_gbc_stop(void);
bool aux_gbc_active(void);
bool aux_gbc_runnable(uint32_t);
void aux_gbc_service(uint32_t);
void aux_gbc_buttons(uint8_t);
uint32_t aux_gbc_stat(unsigned);
