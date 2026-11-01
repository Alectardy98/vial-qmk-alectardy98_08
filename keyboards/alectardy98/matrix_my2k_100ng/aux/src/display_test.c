/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Original procedural test. No asset files, reserved slots, GPIO or keycodes.
 * FPS counts only complete sink transfers, not calls to this renderer. */
#include "aux_display_test.h"
#include "aux_platform.h"
#if MY2K_DEV22_PROFILE
#include "aux_perf22.h"
#endif
static bool active,drawing,failed;
static uint32_t next_pixel,deadline,frames,window_start,window_frames,last_complete;
static uint16_t fraction,fps10;
void aux_display_test_set(bool on,uint32_t now){
 if(on==active)return;
 active=on;drawing=failed=false;next_pixel=frames=window_frames=0;
 deadline=window_start=last_complete=now;fraction=fps10=0;
}
bool aux_display_test_active(void){return active;}
bool aux_display_test_runnable(uint32_t now){return active&&!failed&&(drawing||(int32_t)(now-deadline)>=0);}
void aux_display_test_status(uint8_t out[4]){
 uint16_t value=active&&(uint32_t)(aux_millis()-last_complete)<1000u?fps10:0;
 out[0]=active;out[1]=(uint8_t)value;out[2]=(uint8_t)(value>>8);out[3]=failed;
}
static bool digit(unsigned value,unsigned x,unsigned y){
 static const uint8_t segments[10]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
 if(value>9||x>=8||y>=14)return false;
 uint8_t m=segments[value];
 return ((m&1)&&y<2&&x>=1&&x<7)||((m&2)&&x>=6&&y>=1&&y<7)||
        ((m&4)&&x>=6&&y>=7&&y<13)||((m&8)&&y>=12&&x>=1&&x<7)||
        ((m&16)&&x<2&&y>=7&&y<13)||((m&32)&&x<2&&y>=1&&y<7)||
        ((m&64)&&y>=6&&y<8&&x>=1&&x<7);
}
static uint16_t pixel(unsigned x,unsigned y,uint32_t frame){
 if(y<20){
  bool on=false;
  if(y>=3&&y<17){
   if(x>=4&&x<12)on=digit((fps10/100u)%10u,x-4,y-3);
   else if(x>=15&&x<23)on=digit((fps10/10u)%10u,x-15,y-3);
   else if(x>=25&&x<27&&y>=14)on=true;
   else if(x>=30&&x<38)on=digit(fps10%10u,x-30,y-3);
   /* F P S, simple3x5 bitmap enlarged2x. */
   if(x>=58&&x<82){
    static const uint16_t letters[3]={0x7924,0x7b24,0x79cf};
    unsigned cell=(x-58)/8u,dx=((x-58)%8u)/2u,dy=(y-3)/2u;
    if(cell<3&&dx<3&&dy<5)on|=(letters[cell]>>(14u-dy*3u-dx))&1u;
   }
  }
  return on?0xffff:0;
 }
 unsigned r=(x+frame)&31u,g=(y+frame/2u)&63u,b=(x^y^(frame/2u))&31u;
 uint16_t color=(uint16_t)((r<<11)|(g<<5)|b);
 unsigned bx=(frame*2u)%104u,by=30u+(frame%74u);
 if(x>=bx&&x<bx+24u&&y>=by&&y<by+24u){
  bool edge=x<bx+2||x>=bx+22||y<by+2||y>=by+22;
  return edge?0xffff:0;
 }
 return color;
}
void aux_display_test_step(uint32_t now,aux_pixel_sink sink,void *context){
 if(!active||failed||!sink)return;
 unsigned rate=aux_assets_frame_rate();if(rate!=30&&rate!=60)rate=30;
 if(!drawing){
  if((int32_t)(now-deadline)<0)return;
  if((int32_t)(now-deadline)>100)deadline=now;
  unsigned period=1000u/rate;fraction+=(uint16_t)(1000u%rate);
  if(fraction>=rate){fraction-=rate;period++;}deadline+=period;next_pixel=0;drawing=true;
#if MY2K_DEV22_PROFILE
  aux_perf22_begin((uint16_t)period);
#endif
 }
 uint16_t pixels[256];
#if MY2K_DEV22_PROFILE
 bool scope=aux_perf22_enter(PERF22_DECODE);
#endif
 for(unsigned i=0;i<256;i++){unsigned p=next_pixel+i;pixels[i]=pixel(p%128u,p/128u,frames);}
#if MY2K_DEV22_PROFILE
 aux_perf22_leave(scope);
#endif
 bool okay=sink(context,128,128,next_pixel,pixels,256);next_pixel+=256;
 if(!okay||next_pixel==16384u){
  drawing=false;
  if(!okay)failed=true;
  else{
   frames++;last_complete=aux_millis();
   if(frames==1){window_start=last_complete;window_frames=0;}else window_frames++;
   uint32_t elapsed=last_complete-window_start;
   if(elapsed>=500u){uint32_t value=window_frames*10000u/elapsed;fps10=(uint16_t)(value>65535u?65535u:value);window_frames=0;window_start=last_complete;}
  }
#if MY2K_DEV22_PROFILE
  aux_perf22_end(okay);
#endif
 }
}
