// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>

static inline bool virgo_vectors_valid(uint32_t sp, uint32_t reset) {
  return sp > 0x20000000U && sp <= 0x2001FFF8U && !(sp & 7U) &&
         (reset & 1U) && reset >= 0x08010001U && reset < 0x08040000U;
}

static inline bool virgo_write_valid(uint32_t address, uint32_t length,
                                     uint16_t flash_kib) {
  return flash_kib == 256 && length == 256 && !(address & 255U) &&
         address >= 0x08010000U && address <= 0x08040000U - 256;
}
