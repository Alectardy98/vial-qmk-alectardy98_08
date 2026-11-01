/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_lock_lights.h"
#include "main_lock_store.h"
#include <string.h>
static uint8_t active[76],staged[76],token,received,phase,error,selected,host_locks;
static uint32_t revision,stage_time,selected_at,selection_changes,frames,layers_seen;
static bool legacy,upload;
static my2k_rgb_color output;
static uint32_t get32(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
__attribute__((noinline,noclone,externally_visible)) void my2k_lock_init(bool preserve_legacy){
 my2k_lock_defaults(active);my2k_lock_store_init();bool saved=my2k_lock_store_load(active);
 legacy=!saved&&preserve_legacy;upload=false;token=received=phase=error=host_locks=0;selected=255;
 revision=1;stage_time=selected_at=selection_changes=frames=0;layers_seen=UINT32_MAX;output=(my2k_rgb_color){0,0,0};
}
__attribute__((noinline,noclone,externally_visible)) void my2k_lock_service(uint32_t now,bool allow_flash){
 if(upload&&(uint32_t)(now-stage_time)>5000){upload=false;received=0;}
 if(allow_flash)my2k_lock_store_task();
 if(phase==1&&!my2k_lock_store_pending()){
  uint8_t saved[76];
  if(my2k_lock_store_error()||!my2k_lock_store_load(saved)||memcmp(saved,staged,76)){phase=3;error=7;}
  else{memcpy(active,saved,76);legacy=false;revision++;phase=2;error=0;layers_seen=UINT32_MAX;}
 }
}
__attribute__((noinline,noclone,externally_visible)) bool my2k_lock_hid(uint8_t*p,uint8_t length,uint32_t now,bool allow_writes){
 if(!p||length!=32||p[0]!=253||p[1]!=233)return false;
 uint8_t request[32];memcpy(request,p,32);memset(p,0,32);uint8_t op=request[2],t=request[4];
 p[0]=253;p[1]=233;p[2]=0x55;p[3]=1;p[4]=op;p[5]=t;
 #define FAIL(code) do{p[6]=(code);return true;}while(0)
 if(request[3]!=1||op>6)FAIL(1);
 if(upload&&(uint32_t)(now-stage_time)>5000)upload=false;
 if(op>=2&&op<=4&&!allow_writes)FAIL(2);
 if(op==0){memcpy(p+6,"LKS1",4);p[10]=76;p[11]=8;p[12]=5;p[13]=MY2K_LOCK_LAYER_COUNT;p[14]=legacy;p[15]=1;put32(p+16,revision);p[20]=my2k_lock_store_remaining();p[21]=my2k_lock_store_remaining()>>8;p[22]=my2k_lock_store_error()!=0;}
 else if(op==1){unsigned at=request[5],n=request[6];if(!n||n>24||at>=76||n>76-at)FAIL(3);p[6]=at;p[7]=n;memcpy(p+8,active+at,n);}
 else if(op==2){if(phase==1||upload)FAIL(4);if(!t||get32(request+5)!=revision)FAIL(5);if(my2k_lock_store_error())FAIL(7);token=t;received=0;upload=true;stage_time=now;memset(staged,0,76);}
 else if(op==3){unsigned at=request[5],n=request[6];if(!upload||t!=token||!n||n>24||at!=received||n>76-at)FAIL(3);memcpy(staged+at,request+7,n);received+=n;stage_time=now;}
 else if(op==4){if(!upload||t!=token||received!=76)FAIL(3);upload=false;if(!my2k_lock_validate(staged))FAIL(6);if(!my2k_lock_store_request(staged))FAIL(my2k_lock_store_remaining()?7:8);phase=1;error=0;}
 else if(op==5){p[6]=phase;p[7]=error;p[8]=my2k_lock_store_pending();p[9]=my2k_lock_store_error()!=0;put32(p+10,revision);memcpy(p+14,active+72,4);put32(p+18,my2k_lock_store_commits());p[22]=legacy;}
 else if(op==6){p[6]=selected;p[7]=legacy;p[8]=host_locks;put32(p+9,layers_seen);p[13]=output.r;p[14]=output.g;p[15]=output.b;put32(p+16,frames);put32(p+20,selection_changes);p[24]=112;p[25]=my2k_rgb_channels[112].driver;p[26]=my2k_lock_store_pending();p[27]=my2k_lock_store_error()!=0;}
 p[2]=0xaa;return true;
 #undef FAIL
}
__attribute__((noinline,noclone,externally_visible)) bool my2k_lock_frame(my2k_amk_rgb_runtime*r,uint32_t now,uint8_t locks,uint32_t layers){
 if(layers!=layers_seen){r->dirty=true;layers_seen=layers;}
 if(!my2k_amk_rgb_tick(r,now,locks))return false;
 host_locks=locks;
 if(!legacy){
  uint8_t match;output=my2k_lock_evaluate(active,locks,layers,now-selected_at,&match);
  if(match!=selected){selected=match;selected_at=now;selection_changes++;output=my2k_lock_evaluate(active,locks,layers,0,&match);}
  output.r=(uint16_t)output.r*r->brightness[4]/255u;output.g=(uint16_t)output.g*r->brightness[4]/255u;output.b=(uint16_t)output.b*r->brightness[4]/255u;
  if(!r->enabled)output=(my2k_rgb_color){0,0,0};
  r->rgb.rendered[112]=output;
 }else output=r->rgb.rendered[112];
 frames++;return true;
}
