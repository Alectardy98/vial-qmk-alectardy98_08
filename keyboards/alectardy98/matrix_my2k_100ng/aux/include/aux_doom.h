/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "aux_panels.h"
int      aux_doom_start(aux_panels *);
void     aux_doom_stop(void);
void     aux_doom_buttons(unsigned);
void     aux_doom_service(uint32_t);
bool     aux_doom_active(void);
bool     aux_doom_runnable(uint32_t);
uint32_t aux_doom_stat(unsigned);
