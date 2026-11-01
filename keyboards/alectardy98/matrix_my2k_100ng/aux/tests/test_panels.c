/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "aux_panels.h"
static struct {aux_panel_id id;bool data;size_t n;uint8_t bytes[256];} packets[512];
static unsigned used,delays,signals;static bool dc[2],fail,delay_fail;static unsigned fail_at=~0u;
static bool configure(void *ctx){(void)ctx;return true;}
static bool signal(void *ctx,aux_panel_id id,unsigned line,bool high){(void)ctx;assert(id<=AUX_PANEL_MONO&&line<=AUX_PANEL_POWER);signals++;if(line==AUX_PANEL_DC)dc[id]=high;return true;}
static bool send(void *ctx,aux_panel_id id,const uint8_t *bytes,size_t n){(void)ctx;if(fail||used==fail_at)return false;assert(used<512&&n<=256);packets[used].id=id;packets[used].data=dc[id];packets[used].n=n;memcpy(packets[used++].bytes,bytes,n);return true;}
static bool delay(void *ctx,uint32_t ms){(void)ctx;delays+=ms;return !delay_fail;}
uint32_t aux_millis(void){assert(!"Physical timing accessed in default-off test");return 0;}
bool aux_delay(uint32_t ms){(void)ms;assert(!"Physical delay accessed in default-off test");return false;}
void aux_work_checkpoint(void){}
int main(void){aux_panels p;aux_panel_bus bus={0,configure,signal,send,delay};
 assert(aux_panels_init(&p,&bus)&&p.color_ready&&p.mono_ready&&delays==965&&signals>0);
 assert(packets[0].id==AUX_PANEL_COLOR&&!packets[0].data&&packets[0].n==1&&packets[0].bytes[0]==1);
 assert(packets[used-1].id==AUX_PANEL_MONO&&!packets[used-1].data&&packets[used-1].bytes[0]==0xae);
 /* Binary-derived initialization order: full black visible frame after the
  * settings, before 0x13/0x29 (AUX 0x08002c1c..0x08002c4a). */
 unsigned clear=0,enable=0,total=0;
 for(unsigned i=0;i<used;i++)if(packets[i].id==AUX_PANEL_COLOR&&!packets[i].data){
  if(packets[i].bytes[0]==0x2c)clear=i;
  if(packets[i].bytes[0]==0x29)enable=i;
 }
 assert(clear>4&&enable>clear);
 assert(packets[clear-4].bytes[0]==0x2a&&packets[clear-3].bytes[1]==2&&packets[clear-3].bytes[3]==129);
 assert(packets[clear-2].bytes[0]==0x2b&&packets[clear-1].bytes[1]==1&&packets[clear-1].bytes[3]==128);
 for(unsigned i=clear+1;i<enable;i++)if(packets[i].data){
  assert(packets[i].id==AUX_PANEL_COLOR);total+=(unsigned)packets[i].n;
  for(unsigned j=0;j<packets[i].n;j++)assert(packets[i].bytes[j]==0);
 }
 assert(total==128u*128u*2u);
 used=0;uint16_t colors[]={0xf800,0x07e0,0x001f};assert(aux_panel_color_pixels(&p,128,2,127,colors,3));assert(used==12);
 assert(packets[0].bytes[0]==0x2a&&packets[1].n==4&&packets[1].bytes[1]==129&&packets[1].bytes[3]==129);
 assert(packets[3].bytes[1]==1&&packets[5].n==2&&packets[5].bytes[0]==0xf8&&packets[5].bytes[1]==0);
 assert(packets[7].bytes[1]==2&&packets[7].bytes[3]==3&&packets[9].bytes[1]==2);
 assert(packets[11].n==4&&packets[11].bytes[0]==7&&packets[11].bytes[1]==0xe0&&packets[11].bytes[2]==0&&packets[11].bytes[3]==0x1f);
 unsigned previous=used;assert(!aux_panel_color_pixels(&p,129,2,0,colors,3)&&used==previous);
 uint16_t mono[2800]={0};mono[0]=65535;mono[2799]=65535;used=0;
 for(unsigned first=0;first<2800;first+=128){unsigned n=2800-first;if(n>128)n=128;assert(aux_panel_mono_pixels(&p,70,40,first,mono+first,n));}
 for(unsigned page=0;page<5;page++)aux_panels_service(&p,0);
 assert(used==10&&p.mono[0]==1&&p.mono[4*72+69]==128&&p.mono[4*72+70]==0&&p.mono[4*72+71]==0);
 for(unsigned page=0;page<5;page++){assert(!packets[page*2].data&&packets[page*2].n==3&&packets[page*2].bytes[0]==0x11&&packets[page*2].bytes[1]==0x0c&&packets[page*2].bytes[2]==(0xb0|page));assert(packets[page*2+1].data&&packets[page*2+1].n==72);}
 fail=true;assert(!aux_panel_color_pixels(&p,128,1,0,colors,3)&&!p.color_ready);previous=used;assert(!aux_panel_color_pixels(&p,128,1,0,colors,3)&&used==previous);
 /* A transport failure during the black frame must suppress display-on and
  * never mark the color panel ready. */
 fail=false;used=0;fail_at=clear+2;
 assert(!aux_panels_init(&p,&bus)&&!p.color_ready);
 for(unsigned i=0;i<used;i++)assert(packets[i].id!=AUX_PANEL_COLOR||packets[i].data||packets[i].bytes[0]!=0x29);
 fail_at=~0u;
 /* A stopped tick must not shorten reset timing and then report success. */
 used=0;delay_fail=true;
 assert(!aux_panels_init(&p,&bus)&&!p.color_ready&&!p.mono_ready&&used==0);
 delay_fail=false;
 /* Calling the actual platform default build must stop before ANY MMIO/delay. */
#if !AUX_ENABLE_RECOVERED_PANELS
 aux_panels disabled;assert(!aux_panels_init(&disabled,aux_panel_physical_bus())&&!disabled.color_ready&&!disabled.mono_ready);
#endif
 /* Enabled register path executes only in emulate_worker.py's MMIO model. */
 puts("AUX real panel init/window/RGB565/OLED page packing/failure/default-off gate: PASS");return 0;}
