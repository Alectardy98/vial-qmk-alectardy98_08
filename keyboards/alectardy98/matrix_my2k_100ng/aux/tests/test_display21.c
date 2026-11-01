/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "aux_panels.h"
#include "aux_perf21.h"
static uint32_t now;static unsigned calls,bytes,windows;static bool data,fail;
uint32_t aux_millis(void){return now;}
static bool signal(void*c,aux_panel_id id,unsigned line,bool high){(void)c;assert(id==AUX_PANEL_COLOR);if(line==AUX_PANEL_DC)data=high;return true;}
static bool send(void*c,aux_panel_id id,const uint8_t*p,size_t n){(void)c;assert(id==AUX_PANEL_COLOR);calls++;if(fail)return false;if(!data&&n==1&&p[0]==0x2c)windows++;if(data&&n==256){for(unsigned i=0;i<128;i++){assert(p[2*i]==(uint8_t)(i>>8));assert(p[2*i+1]==(uint8_t)i);}bytes+=n;}return true;}
int main(void){aux_panels p={.bus={.signal=signal,.send=send},.color_ready=true,.color_capable=true,.color_on=true,.color_target=true};uint16_t pix[128];for(unsigned i=0;i<128;i++)pix[i]=i;
 now=100;for(unsigned i=0;i<128;i++){assert(aux_panel_color_pixels(&p,128,128,i*128,pix,128));now++;}
 assert(aux_display_perf.frames==1&&aux_display_perf.last_frame_ms==127&&aux_display_perf.total_frame_ms==127&&aux_display_perf.histogram[4]==1&&bytes==32768&&windows==128);
 now=500;assert(aux_panel_color_pixels(&p,128,128,0,pix,128));fail=true;assert(!aux_panel_color_pixels(&p,128,128,128,pix,128));assert(aux_display_perf.frames==1&&aux_display_perf.failed_frames==1&&!aux_display_perf.frame_active);
 unsigned old=calls;assert(!aux_panel_color_pixels(&p,128,128,256,pix,128)&&old==calls);
 puts("PASS D015 pixel encoding/windows preserved, drained-completion counters, failure not counted as completed frame");
}
