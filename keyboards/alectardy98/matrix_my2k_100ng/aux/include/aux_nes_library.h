/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Called only by the filesystem worker, never by USB callbacks. */
bool    aux_nes_library_selected(char path[160]);
uint8_t aux_nes_library_request(unsigned op, uint32_t cursor, const uint8_t *data, unsigned size, uint8_t *out,
                                unsigned *out_size);
/* 0 no save,1 valid loaded/saved,-1 error. Workspace is borrowed at game boundary. */
int  aux_nes_library_ram(const char *name, uint8_t ram[8192], uint8_t scratch[8192], bool save);
bool aux_nes_library_ready(void);
/* Filesystem-worker-only cartridge-save lease, never an external MSC writer. */
bool aux_nes_library_writing(void);
void aux_nes_library_close_failed(void);

bool    aux_gbc_library_selected(char path[160]);
uint8_t aux_gbc_library_request(unsigned op, uint32_t cursor, const uint8_t *data, unsigned size, uint8_t *out,
                                unsigned *out_size);
int aux_gbc_library_ram(const char *name, uint8_t *ram, uint8_t *scratch, unsigned bytes, uint8_t rtc[5], bool save);
