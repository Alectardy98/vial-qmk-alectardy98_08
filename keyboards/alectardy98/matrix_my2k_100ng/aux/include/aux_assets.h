/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef AUX_ASSETS_H
#define AUX_ASSETS_H
#include "aux_stock_parity.h"
#include <stdint.h>
#include <stdbool.h>
bool aux_assets_set_frame_rate(uint8_t rate);
uint8_t aux_assets_frame_rate(void);
bool aux_assets_set_timing(uint8_t policy); /*0 original duration,1 fixed rate*/
uint8_t aux_assets_timing(void);
void aux_assets_set_test(bool enabled);
/* Real decoded frame sinks. Returning false reports unavailable/failing output. */
typedef bool (*aux_pixel_sink)(void *ctx,uint16_t width,uint16_t height,uint32_t first,const uint16_t *pixels,unsigned count);
typedef struct {aux_pixel_sink color,mono;void *context;} aux_render_sinks;
bool aux_assets_init(void);
/* Each step decodes at most128 pixels per channel; caller services transports between steps. */
void aux_assets_step(uint32_t now,const aux_render_sinks *sinks);
bool aux_assets_busy(void);
/* Select a validated root-directory ANIM by filename; no filesystem writes. */
bool aux_assets_select_color(const char *filename);
/* Release startup presentation only after Main resolves its persistent slot.
 * With no saved slot, Main explicitly accepts the discovered default. */
void aux_assets_allow_color(void);
/* Retain the displayed frame while a game owns or is acquiring the panel. */
void aux_assets_hold_color(void);
#if MY2K_ANIMATION_PROFILE
const char *aux_assets_selected_color(void);
bool aux_assets_release_color(void);
#endif
#if AUX_STOCK_PARITY
/* Worker-only, after completed host write+SYNC. Reopens validated BW_TEXT and
 * invalidates its file/volume cache; decoding remains cooperatively chunked. */
bool aux_assets_reload_mono(void);
bool aux_assets_media_service(void);
#endif
/* RAM-only scheduling controls, not physical panel/firmware protocol commands. */
void aux_assets_set_paused(bool paused,uint32_t now);
/* Inactive outputs stop file reads. A color restart redraws from pixel zero
 * because a power cycle clears controller RAM; mono RAM survives blanking. */
void aux_assets_set_outputs(bool color_enabled,bool mono_enabled,uint32_t now);
typedef struct {
    bool mounted,paused,color_present,mono_present,color_active,mono_active;
    bool color_decoding,mono_decoding;
    uint16_t color_frame,color_width,color_height,mono_width,mono_height;
    uint32_t color_next_pixel,mono_next_pixel,color_completed,mono_completed;
    uint32_t output_failures,read_errors;
    /* Per-channel failures prevent an unrelated LCD error failing OLED reload. */
    uint32_t mono_read_errors,mono_output_failures,color_read_errors,color_output_failures;
    uint32_t mount_attempts,mount_failures,mono_open_failures,color_open_failures;
    uint8_t last_mount_result,last_mono_open_result,last_color_open_result;
} aux_asset_status;
void aux_assets_status(aux_asset_status *out);
extern volatile uint32_t aux_decoded_color_frames,aux_decoded_mono_frames;
extern volatile uint32_t aux_render_failures,aux_asset_errors;
#endif

#if MY2K_RELEASE1_PROFILE
bool aux_assets_export_begin(void);
#endif

#if MY2K_RELEASE1_PROFILE
bool aux_assets_release_mono(void);
#endif
