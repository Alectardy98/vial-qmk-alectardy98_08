/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_NES_CORE_H
#define MY2K_NES_CORE_H
#include <stddef.h>
#include <stdint.h>
#include "my2k_nes_rom.h"
/* CPU/PPU core owns no hardware. Sinks consume one bounded 128-pixel row.
 * The caller supplies ROM bytes for the entire session; no ROM is copied. */
#define MY2K_NES_ROM_MAX (96u*1024u+528u)
typedef void (*my2k_nes_line_sink)(unsigned row,const uint16_t *rgb565);
void my2k_nes_core_callbacks(my2k_nes_line_sink,void (*checkpoint)(void));
int my2k_nes_core_load(uint8_t *rom,size_t bytes);
typedef int (*my2k_nes_rom_reader)(uint32_t offset,void *dst,unsigned bytes);
/* Streaming workspace replaces the existing ROM buffer; never allocate per ROM. */
int my2k_nes_core_load_stream(const uint8_t header[16],uint32_t bytes,
                            uint8_t *workspace,size_t workspace_bytes,my2k_nes_rom_reader);
uint8_t *my2k_nes_prg_page(unsigned window,unsigned bank);
uint8_t *my2k_nes_chr_page(unsigned window,unsigned bank);
unsigned my2k_nes_cache_misses(void);
unsigned my2k_nes_cache_hits(void);
uint8_t *my2k_nes_battery_ram(void);
void my2k_nes_core_step(int draw);
void my2k_nes_core_stop(void);
void my2k_nes_core_buttons(uint8_t);
unsigned my2k_nes_core_heap_used(void);
unsigned my2k_nes_core_fault(void);
#endif
