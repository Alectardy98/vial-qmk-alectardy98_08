/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_text.h"
#include <string.h>
/* Reuse the repository's QMK font read-only. No manufacturer font extraction.
 * platforms/progmem.h supplies the ordinary-memory ARM/host definition. */
#include "glcdfont.c"

static void remember(aux_text *text) {
    if (!text->current) return;
    if (text->count == sizeof text->history) {
        memmove(text->history, text->history + 1, sizeof text->history - 1);
        text->history[sizeof text->history - 1] = text->current;
    } else text->history[text->count++] = text->current;
}

bool aux_text_apply(aux_text *text, const my2k_wire_message *message) {
    if (!text || !message || message->type != MY2K_TX) return false;
#if MY2K_RELEASE1_PROFILE
    if(!text->typing_enabled)return true;
#endif
    /* Validate before mutating so direct callers cannot leave a partial event. */
    char encoded[32];
    if (my2k_wire_encode(message, encoded, sizeof encoded) < 0) return false;
    if (!message->v[0]) {
        remember(text);
        text->current = 0;
    } else for (unsigned i = 0; i < 8 && message->v[i]; i++) {
        remember(text);
        text->current = message->v[i];
    }
    text->dirty = true;
    return true;
}

#if MY2K_RELEASE1_PROFILE
#define glyph aux_text_glyph
void glyph(
#else
static void glyph(
#endif
uint8_t *frame, unsigned x, unsigned y, uint8_t ch, unsigned scale) {
    for (unsigned col = 0; col < 5; col++) {
        uint8_t bits = font[(unsigned)ch * 6 + col];
        for (unsigned row = 0; row < 8; row++) if (bits & (1u << row)) {
            for (unsigned dx = 0; dx < scale; dx++) for (unsigned dy = 0; dy < scale; dy++) {
                unsigned px = x + col * scale + dx, py = y + row * scale + dy;
                if (px < 72 && py < 40) frame[py / 8 * 72 + px] |= (uint8_t)(1u << (py & 7));
            }
        }
    }
}
#if AUX_STOCK_PARITY
static void line(uint8_t *frame,unsigned x,unsigned y,const char *text){for(unsigned i=0;text[i];i++)glyph(frame,x+6u*i,y,(uint8_t)text[i],1);}
static void pair(char *out,unsigned value){out[0]=(char)('0'+value/10u%10u);out[1]=(char)('0'+value%10u);}
void aux_text_clock_render(const aux_datetime *clock,aux_panels *panels){
 if(!clock||!panels||!panels->mono_ready)return;
 memset(panels->mono_overlay,0,sizeof panels->mono_overlay);
 if(!clock->valid)line(panels->mono_overlay,6,14,"SYNC CLOCK");
 else{
  char date[]="0000-00-00",time[]="00:00:00";
  pair(date,clock->year/100u);pair(date+2,clock->year);pair(date+5,clock->month);pair(date+8,clock->day);
  pair(time,clock->hour);pair(time+3,clock->minute);pair(time+6,clock->second);
  static const char weekdays[7][4]={"MON","TUE","WED","THU","FRI","SAT","SUN"};
  line(panels->mono_overlay,6,2,date);line(panels->mono_overlay,12,16,time);
  line(panels->mono_overlay,27,30,weekdays[clock->weekday-1]);
 }
 panels->mono_overlay_enabled=true;panels->mono_dirty=true;panels->mono_page=0;
}
#endif

void aux_text_service(aux_text *text, aux_panels *panels) {
    if (!text || !panels || !text->dirty || !panels->mono_ready || panels->mono_dirty) return;
    if (!text->current) {
        panels->mono_overlay_enabled = false;
    } else {
        memset(panels->mono_overlay, 0, sizeof panels->mono_overlay);
#if MY2K_RELEASE1_PROFILE
        for(unsigned i=0;i<10&&text->label[i];i++)glyph(panels->mono_overlay,i*6,0,(uint8_t)text->label[i],1);
#endif
        /* Stock anchors history at (7-count)*6 and current at x44. The QMK
         * glyphs and 2x enlargement are independent presentation choices. */
        for (unsigned i = 0; i < text->count; i++)
            glyph(panels->mono_overlay, (7 - text->count + i) * 6, 28, text->history[i], 1);
        glyph(panels->mono_overlay, 44, 20, text->current, 2);
        panels->mono_overlay_enabled = true;
    }
    panels->mono_dirty = true;
    panels->mono_page = 0;
    text->dirty = false;
}
