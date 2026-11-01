/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
void my2k_main_rgb_diag(unsigned group, unsigned char reply[32]);
#include <stdint.h>
#include <stdbool.h>
void my2k_diag_set_attempt(const uint8_t *packet);
void my2k_diag_set_complete(void);
void my2k_diag_key(uint8_t row, uint8_t col, uint16_t code, bool pressed);
bool my2k_diag_reply(uint8_t *packet);
