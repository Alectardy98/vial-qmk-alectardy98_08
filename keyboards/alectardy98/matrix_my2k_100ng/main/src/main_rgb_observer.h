/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "my2k_rgb.h"
/* Diagnostic-only. Captures register contents, never changes PWM, enables,
 * shutdown, current or persistent configuration. Bank selections restore0. */
void my2k_rgb_observer_key(uint8_t row,uint8_t col,uint16_t code,bool pressed);
void my2k_rgb_observer_service(const my2k_rgb_state *state,uint32_t now);
bool my2k_rgb_observer_page(uint8_t page,uint8_t index,uint8_t report[32]);
bool my2k_rgb_observer_bank_safe(unsigned chip);
#if MY2K_RGB_OBSERVER_HOST
int my2k_rgb_observer_write(uint8_t addr,uint8_t reg,const uint8_t *data,uint16_t count,uint16_t timeout);
int my2k_rgb_observer_read(uint8_t addr,uint8_t reg,uint8_t *data,uint16_t count,uint16_t timeout);
uint32_t my2k_rgb_observer_clock(void);
#endif
