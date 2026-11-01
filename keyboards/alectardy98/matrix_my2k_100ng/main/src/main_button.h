/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
/* Cached PB5 input sample only; no register reads or mode changes. */
void my2k_button_status(uint8_t report[32]);
