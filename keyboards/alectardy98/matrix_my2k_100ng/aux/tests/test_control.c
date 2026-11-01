/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "aux_control.h"
static bool dc[2],runtime,fail_send,power_high;
static unsigned color_bytes,mono_bytes,black_bytes,display_on,delays;
static uint8_t color_command,mono_page_number,shown[360];
static bool configure(void *ctx){(void)ctx;return true;}
static bool signal_line(void *ctx,aux_panel_id id,unsigned line,bool high){
 (void)ctx;if(line==AUX_PANEL_DC)dc[id]=high;
 if(id==AUX_PANEL_COLOR&&line==AUX_PANEL_POWER)power_high=high;
 return true;
}
static bool send_bytes(void *ctx,aux_panel_id id,const uint8_t *b,size_t n){
 (void)ctx;if(fail_send)return false;
 if(id==AUX_PANEL_COLOR){
  if(!dc[id]){assert(n==1);color_command=b[0];if(b[0]==0x29)display_on++;}
  else{
   color_bytes+=(unsigned)n;
   if(color_command==0x2c){for(size_t i=0;i<n;i++)assert(!b[i]);black_bytes+=(unsigned)n;}
  }
 }else if(!dc[id]){if(n==3)mono_page_number=b[2]&7;}
 else{assert(n==72&&mono_page_number<5);memcpy(shown+mono_page_number*72,b,n);mono_bytes+=(unsigned)n;}
 return true;
}
static bool delay_ms(void *ctx,uint32_t n){(void)ctx;assert(!runtime);delays+=n;return true;}
static uint32_t now=UINT32_MAX-50u;
static void step(aux_panels *p){
 color_bytes=mono_bytes=0;aux_panels_service(p,now++);
 assert(color_bytes<=128&&mono_bytes<=72);
}
static int finish(aux_control *c){
 unsigned iterations=0;int result=0;
 do{aux_text_service(&c->text,c->panels);step(c->panels);int next=aux_control_result(c);if(next){assert(!result);result=next;}
    assert(++iterations<2000);
 }while(aux_panels_busy(c->panels));
 return result;
}
static bool request(aux_control *c,const char *frame,bool *reply){
 my2k_wire_message m;assert(my2k_wire_parse(frame,strlen(frame),&m));
 return aux_control_apply(c,&m,reply);
}
int main(void){
 aux_panel_bus bus={0,configure,signal_line,send_bytes,delay_ms};aux_panels p;
 assert(aux_panels_init(&p,&bus));assert(delays==965);runtime=true;
 aux_control c={.panels=&p};bool reply;black_bytes=0;
 assert(request(&c,"SCR:PWR=OFF;\n",&reply)&&!reply);
 assert(aux_control_result(&c)==0);
 assert(!request(&c,"SCR:MODE=1;\n",&reply)&&reply); /* Explicit busy response. */
 assert(finish(&c)==1&&!p.color_on&&!p.color_ready&&power_high&&black_bytes==32768);
 assert(request(&c,"SCR:PWR=OFF;\n",&reply)&&!reply);
 assert(aux_control_result(&c)==1&&!aux_panels_busy(&p));
 black_bytes=0;unsigned enabled=display_on;
 assert(request(&c,"SCR:MODE=255;\n",&reply)&&!reply); /* Argument is ignored by stock. */
 assert(finish(&c)==1&&p.color_on&&p.color_ready&&!power_high);
 assert(black_bytes==32768&&display_on==enabled+1);
 /* OLED OFF is a blank image, not a guessed power-down opcode. ON restores
  * the retained RAM image; partial asset buffers are not destroyed. */
 uint16_t pixels[8]={65535,0,65535,0,0,0,0,65535};
 assert(aux_panel_mono_pixels(&p,8,1,0,pixels,8));
 assert(finish(&c)==0&&shown[0]==1&&shown[2]==1&&shown[7]==1);
 assert(request(&c,"PWR:OFF;\n",&reply)&&!reply);
 assert(finish(&c)==0&&!p.color_on&&!p.mono_enabled);
 for(unsigned i=0;i<sizeof shown;i++)assert(!shown[i]);
 assert(p.mono[0]==1&&p.mono[2]==1&&p.mono[7]==1);
 assert(request(&c,"PWR:ON;\n",&reply)&&!reply);
 assert(finish(&c)==0&&p.color_on&&p.mono_enabled&&shown[7]==1);
 assert(request(&c,"SCR:AM=3;\n",&reply)&&!reply&&finish(&c)==1&&!p.mono_enabled);
 assert(request(&c,"SCR:AM=0;\n",&reply)&&!reply&&finish(&c)==1&&shown[7]==1);
 assert(request(&c,"TX:A;B;\n",&reply)&&!reply&&finish(&c)==0);
 assert(p.mono_overlay_enabled&&c.text.current=='B'&&c.text.history[0]=='A');
 assert(request(&c,"TX:OFF;\n",&reply)&&!reply&&finish(&c)==0&&shown[7]==1);
 assert(!request(&c,"SCR:AM=1;\n",&reply)&&reply); /* No fake clock renderer. */
 assert(!request(&c,"SCR:DSP=1;\n",&reply)&&reply); /* Playlist policy is pending. */
 assert(request(&c,"SCR:MSC=ON;\n",&reply)&&reply);
 assert(request(&c,"SCR:ADJ=3;\n",&reply)&&reply);
 /* New PWR can supersede a pending SCR without falsely acknowledging it. */
 assert(request(&c,"SCR:PWR=OFF;\n",&reply)&&!reply);
 step(&p);assert(request(&c,"PWR:ON;\n",&reply)&&!reply);
 assert(finish(&c)==-1&&p.color_on);
 /* A transport failure completes the pending response as FAIL and prevents
  * further color writes; this does not suppress independent OLED control. */
 assert(request(&c,"SCR:PWR=OFF;\n",&reply)&&!reply);
 step(&p);fail_send=true;
 assert(finish(&c)==-1&&p.color_fault&&!p.color_ready);
 assert(!request(&c,"SCR:PWR=ON;\n",&reply)&&reply);
 fail_send=false;
 assert(!request(&c,"PWR:OFF;\n",&reply)&&!reply&&finish(&c)==0&&!p.mono_enabled);
 aux_panels disabled={0};c=(aux_control){.panels=&disabled};
 assert(!request(&c,"SCR:PWR=ON;\n",&reply)&&reply);
 assert(!request(&c,"PWR:ON;\n",&reply)&&!reply);
 assert(!request(&c,"TX:A;\n",&reply)&&!reply);
 puts("AUX wire-to-panel power/blank/restore, delayed ACK, bounded work, timer wrap, cancellation and failures: PASS");
 return 0;
}
