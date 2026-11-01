/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef AUX_TEXT_H
#define AUX_TEXT_H
#include "aux_stock_parity.h"
#include "aux_panels.h"
#if AUX_STOCK_PARITY
#include "aux_clock.h"
void aux_text_clock_render(const aux_datetime *,aux_panels *);
#endif
#include "my2k_wire.h"
typedef struct {
    uint8_t history[7], count, current;
    bool dirty;
#if MY2K_RELEASE1_PROFILE
    bool typing_enabled;
    char label[11];
#endif
} aux_text;
/* Recovered TX typing-history behavior; rendering uses QMK's existing font,
 * not the manufacturer's glyphs. OFF retains history and restores the asset. */
bool aux_text_apply(aux_text *text, const my2k_wire_message *message);
/* Coalesces to the latest text state after the prior five-page flush finishes.
 * Never sleeps, reads storage, or sends panel data itself. */
void aux_text_service(aux_text *text, aux_panels *panels);
#endif

#if MY2K_RELEASE1_PROFILE
void aux_text_glyph(uint8_t *,unsigned,unsigned,uint8_t,unsigned);
#endif
