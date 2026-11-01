/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_animation.h"
#include "aux_animation_files.h"
#include "aux_display_test.h"
#include "aux_assets.h"
#include "aux_msc.h"
#include "my2k_asset.h"
#include "ff.h"
#include <string.h>
static uint8_t active;
static bool reader(void *ctx,uint32_t at,void *out,size_t n){UINT got;return f_lseek(ctx,at)==FR_OK&&f_read(ctx,out,n,&got)==FR_OK&&got==n;}
static bool info(unsigned slot,FILINFO *out){
 char path[64];const char *p;
 if(slot){if(!aux_animation_slot_path(slot,path))return false;p=path;}else p=aux_assets_selected_color();
 if(!p||!*p||f_stat(p,out)!=FR_OK||(out->fattrib&AM_DIR))return false;
 FIL f;if(f_open(&f,p,FA_READ)!=FR_OK)return false;
 my2k_reader r={&f,(uint32_t)f_size(&f),reader};my2k_asset a;
 bool okay=f_size(&f)<=UINT32_MAX&&my2k_asset_open(&r,&a)&&a.kind==MY2K_ASSET_ANIM&&a.width==128&&a.height==128;
 return f_close(&f)==FR_OK&&okay;
}
bool aux_animation_request(const my2k_wire_message *q,my2k_wire_message *r){
 if(!q||!r||q->type!=MY2K_AC||q->v[0]<8||q->v[0]>12)return false;
 *r=(my2k_wire_message){.type=MY2K_AR};memcpy(r->v,q->v,3);r->v[3]=1;
 if(!q->v[1])return true;
 if(q->v[0]==8){if(q->v[2]!=1)return true;r->v[3]=0;memcpy(r->v+4,
#if AUX_FRAME_RATE_CONTROL
 "ANI4",
#else
 "ANI1",
#endif
 4);return true;}
 if(q->v[0]==11){
  if(q->v[3]>1||(!q->v[2]&&q->v[3])||(q->v[2]&&q->v[2]!=30&&q->v[2]!=60))return true;
  if(q->v[2]){(void)aux_assets_set_frame_rate(q->v[2]);(void)aux_assets_set_timing(q->v[3]);}
  r->v[3]=0;r->v[4]=aux_assets_frame_rate();r->v[5]=60;r->v[6]=aux_assets_timing();return true;
 }
 if(q->v[0]==12){
  if(q->v[2]>2||q->v[3])return true;
  if(q->v[2]<2)aux_assets_set_test(q->v[2]!=0);
  aux_display_test_status(r->v+4);r->v[3]=0;return true;
 }
 if(aux_animation_files_busy()||aux_msc_export_active()||aux_msc_media_writing()||!aux_assets_media_service()){r->v[3]=2;return true;}
 if(q->v[0]==10){
  char p[64];const char *current=aux_assets_selected_color();if((q->v[2]&&aux_animation_slot_path(q->v[2],p)&&strstr(p,"/anamations/"))||(!q->v[2]&&current&&strchr(current,'/'))){r->v[3]=4;return true;}
  FILINFO f;if(q->v[2]>10||q->v[3]>3||!info(q->v[2],&f)){r->v[3]=3;return true;}
  if(!q->v[3]){for(unsigned i=0;i<4;i++)r->v[4+i]=(uint8_t)(f.fsize>>(8*i));}
  else{const char *s=f.altname[0]?f.altname:f.fname;size_t n=strlen(s);if(n>12){r->v[3]=4;return true;}unsigned at=(q->v[3]-1u)*4u;for(unsigned i=0;i<4;i++)r->v[4+i]=at+i<n?s[at+i]:0;}
  r->v[3]=0;return true;
 }
 active=0;const char *current=aux_assets_selected_color();
 for(unsigned i=1;i<=10;i++){char path[64];if(!aux_animation_slot_path(i,path))continue;if(current&&!strcmp(current,path+3)){active=(uint8_t)i;break;}}
 uint16_t mask=0;for(unsigned i=1;i<=10;i++){FILINFO f;if(info(i,&f))mask|=(uint16_t)(1u<<(i-1u));}
 unsigned target=q->v[2];
 if(target==254||target==255){
  int direction=target==255?1:-1;target=active?active:(direction>0?0u:1u);
  for(unsigned i=0;i<10;i++){target=(unsigned)((int)target-1+direction+10)%10+1u;if(mask&(1u<<(target-1)))break;}
 }
 if(target){char path[64];if(!aux_animation_slot_path(target,path)||!(mask&(1u<<(target-1)))||!aux_assets_select_color(path+3)){r->v[3]=3;return true;}active=(uint8_t)target;}
 r->v[3]=0;r->v[4]=active;r->v[5]=(uint8_t)mask;r->v[6]=(uint8_t)(mask>>8);return true;
}
