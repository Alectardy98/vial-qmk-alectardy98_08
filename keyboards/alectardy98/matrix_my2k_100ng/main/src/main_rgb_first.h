/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
/* Dev21: task-context passive event recorder. No recovery or configuration API. */
enum {
    FIRST_KEY = 1,
    FIRST_IO_BEGIN,
    FIRST_IO_END,
    FIRST_SHUTDOWN,
    FIRST_FAULT,
    FIRST_REGISTERS,
    FIRST_GAP,
    FIRST_AUX,
    FIRST_MODE,
    FIRST_PAYLOAD_HASH
};
void my2k_first_event(uint8_t type, uint8_t address, uint8_t bank, uint8_t reg, uint16_t count, uint8_t status,
                      uint8_t fault, uint32_t detail, const uint8_t *data);
void my2k_first_key(uint8_t row, uint8_t col, uint16_t code, bool pressed, uint32_t layers, uint8_t mods, uint8_t weak,
                    uint8_t default_layer);
void my2k_first_trigger(uint8_t reason, uint8_t address, uint8_t reg);
void my2k_first_rearm(void);
void my2k_first_service(uint32_t now);
bool my2k_first_page(uint8_t page, uint8_t index, uint8_t out[32]);
void my2k_first_health(uint8_t address, uint8_t value, bool valid);
