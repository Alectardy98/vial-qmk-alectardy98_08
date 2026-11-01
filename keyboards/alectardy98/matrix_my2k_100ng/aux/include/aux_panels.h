/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef AUX_PANELS_H
#define AUX_PANELS_H
#include "aux_assets.h"
#include <stddef.h>
typedef enum {AUX_PANEL_COLOR=0,AUX_PANEL_MONO=1} aux_panel_id;
typedef struct {
 void *context;
 bool (*configure)(void *context);
 bool (*signal)(void *context,aux_panel_id panel,unsigned signal,bool high);
 bool (*send)(void *context,aux_panel_id panel,const uint8_t *bytes,size_t size);
 bool (*delay)(void *context,uint32_t milliseconds);
} aux_panel_bus;
enum {AUX_PANEL_CS,AUX_PANEL_DC,AUX_PANEL_RESET,AUX_PANEL_POWER};
typedef struct {
 aux_panel_bus bus;
 bool color_ready,mono_ready;
 uint8_t mono[360];
 /* Separate overlay keeps the partially decoded asset intact. Visibility
  * blanking takes precedence over both images. */
 uint8_t mono_overlay[360];
 bool mono_overlay_enabled;
 bool color_capable,color_on,color_target,color_fault,color_waiting;
 uint8_t color_phase,color_setting;
 uint32_t color_deadline;
 unsigned color_clear;
 bool mono_enabled,mono_dirty;
 uint8_t mono_page;
 bool color_streaming;
 uint16_t stream_width,stream_height;
 uint32_t stream_next;
 bool color_blanked,mono_blanked;
} aux_panels;
/* Worker-only visibility: retained GRAM, no reset, power GPIO or init. */
bool aux_panels_visual_sleep(aux_panels *,bool asleep);
bool aux_panels_init(aux_panels *panels,const aux_panel_bus *bus);
/* Requests never delay or touch the bus. Service sends at most one 128-byte
 * color chunk and one 72-byte OLED page per call, and never sleeps. */
bool aux_panel_request_color_power(aux_panels *panels,bool on);
bool aux_panel_request_mono_visible(aux_panels *panels,bool visible);
bool aux_panel_color_busy(const aux_panels *panels);
bool aux_panels_busy(const aux_panels *panels);
void aux_panels_service(aux_panels *panels,uint32_t now);
bool aux_panel_color_pixels(void *context,uint16_t width,uint16_t height,uint32_t first,const uint16_t *pixels,unsigned count);
bool aux_panel_mono_pixels(void *context,uint16_t width,uint16_t height,uint32_t first,const uint16_t *pixels,unsigned count);
/* Physical pin access is disabled unless explicit offline build option AUX_PANELS=1. */
const aux_panel_bus *aux_panel_physical_bus(void);
#endif
