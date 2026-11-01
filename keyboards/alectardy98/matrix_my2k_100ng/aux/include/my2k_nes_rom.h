/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include <stddef.h>
/* Resource ceilings, not a claim that every cartridge below them is supported. */
#define MY2K_NES_PRG_MAX (4u * 1024u * 1024u)
#define MY2K_NES_CHR_MAX (2u * 1024u * 1024u)
typedef struct {
    uint32_t prg_offset, prg_bytes, chr_offset, chr_bytes, prg_ram, chr_ram;
    uint16_t mapper;
    uint8_t  submapper, flags, nes2;
} my2k_nes_rom_info;
/* 0 valid;2 bad/truncated;3 format;4 mapper;5 lengths;6 board;9 RAM;10 timing. */
int my2k_nes_rom_parse(const uint8_t header[16], uint32_t size, my2k_nes_rom_info *);
