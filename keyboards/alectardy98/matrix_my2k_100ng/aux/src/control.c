/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Original behavior anchors: AUX dispatcher 0x08002e8c, color power 0x08002a08,
 * mono mode 0x080047a8/render 0x080043dc. Independent bounded implementation. */
#include "aux_control.h"
#if AUX_STOCK_PARITY
#include "aux_assets.h"
#include "aux_platform.h"
#endif
bool aux_control_apply(aux_control *c,const my2k_wire_message *m,bool *reply){
 if(!c||!c->panels||!m||!reply)return false;
 *reply=false;
#if AUX_STOCK_PARITY
 if(m->type==MY2K_TM){bool ok=aux_datetime_set(&c->clock,m,aux_millis());if(ok){c->clock_shown=false;
#if MY2K_RELEASE1_PROFILE
 c->oled.clock_until=aux_millis()+3000u;c->oled.clock_overlay=true;c->oled.dirty=true;
 uint8_t stamp[8]={(uint8_t)m->year,(uint8_t)(m->year>>8)};for(unsigned i=0;i<6;i++)stamp[2+i]=m->v[i];
 c->oled.clock_crc=my2k_oled_crc(stamp,8);
#endif
 }return ok;}
#endif
 if(m->type==MY2K_TX)return c->panels->mono_ready&&aux_text_apply(&c->text,m);
 if(m->type==MY2K_PWR){
  if(m->v[0]>1)return false;
  bool color=aux_panel_request_color_power(c->panels,m->v[0]!=0);
  bool mono=aux_panel_request_mono_visible(c->panels,m->v[0]!=0);
  return color&&mono;
 }
 if(m->type!=MY2K_SCR)return false;
 *reply=true;
 if(c->reply_pending)return false;
 bool target;
 switch(m->v[0]){
 case MY2K_SCREEN_PWR:
 case MY2K_SCREEN_MODE:
  if(m->v[0]==MY2K_SCREEN_PWR&&m->v[1]>1)return false;
  /* Stock MODE ignores its numeric argument and toggles current color power. */
  target=m->v[0]==MY2K_SCREEN_MODE?!c->panels->color_target:m->v[1]!=0;
  if(!aux_panel_request_color_power(c->panels,target))return false;
  c->reply_color=true;break;
 case MY2K_SCREEN_AM:
#if AUX_STOCK_PARITY
  /* GUI mode0 reloads the uploaded ABIT, mode1 shows the synchronized clock,
   * mode3 blanks. The absent thermometer mode2 is explicitly unsupported. */
  if(m->v[1]!=0&&m->v[1]!=1&&m->v[1]!=3)return false;
  if(!c->panels->mono_ready)return false;
  c->reply_asset=false;
  if(m->v[1]==0){
   if(!aux_assets_reload_mono())return false;
   aux_asset_status status;aux_assets_status(&status);
   c->reply_asset=true;c->reload_completed=status.mono_completed;
   c->reload_errors=status.mono_read_errors;c->reload_output_failures=status.mono_output_failures;
  }
  c->mono_mode=m->v[1];c->clock_shown=false;c->text.current=0;c->text.dirty=false;
  c->panels->mono_overlay_enabled=false;
  target=m->v[1]!=3;
  if(!aux_panel_request_mono_visible(c->panels,target))return false;
  c->reply_color=false;break;
#else
  /* 0 = stored text, 3 = blank. Clock/thermometer rendering remains unsupported.
   * Restoring RAM avoids opening files during command processing. */
  if(m->v[1]!=0&&m->v[1]!=3)return false;
  target=m->v[1]==0;
  if(!aux_panel_request_mono_visible(c->panels,target))return false;
  c->reply_color=false;break;
#endif
 case MY2K_SCREEN_MSC:
 case MY2K_SCREEN_ADJ:
  /* Verified stock dispatcher ACKs these without a corresponding action. */
  return true;
 default:return false;
 }
 c->target=target;c->reply_pending=true;
#if AUX_STOCK_PARITY
 c->reply_deadline=aux_millis()+3000u;
#endif
 *reply=false;return true;
}
int aux_control_result(aux_control *c){
 if(!c||!c->panels||!c->reply_pending)return 0;
#if AUX_STOCK_PARITY
 if((int32_t)(aux_millis()-c->reply_deadline)>=0){c->reply_pending=false;c->reply_asset=false;return -1;}
#endif
 aux_panels *p=c->panels;bool ok;
 if(c->reply_color){
  if(aux_panel_color_busy(p))return 0;
  ok=!p->color_fault&&p->color_on==c->target;
 }else{
#if AUX_STOCK_PARITY
  if(c->reply_asset){
   aux_asset_status status;aux_assets_status(&status);
   if(status.mono_read_errors!=c->reload_errors||status.mono_output_failures!=c->reload_output_failures||(!status.mono_active&&status.mono_completed==c->reload_completed)){
    c->reply_asset=false;c->reply_pending=false;return -1;
   }
   if(status.mono_completed==c->reload_completed)return 0;
   c->reply_asset=false;
  }
#endif
  if(p->mono_ready&&p->mono_dirty)return 0;
  ok=p->mono_ready&&p->mono_enabled==c->target;
 }
 c->reply_pending=false;return ok?1:-1;
}
void aux_control_service(aux_control *c,uint32_t now){
#if AUX_STOCK_PARITY
 if(!c||!c->panels)return;
 bool changed=aux_datetime_step(&c->clock,now);
#if MY2K_RELEASE1_PROFILE
 (void)changed;
#endif
#if MY2K_RELEASE1_PROFILE
 aux_oled_service(&c->oled,&c->text,&c->clock,c->panels,now);
#else
 if(c->mono_mode==1&&!c->text.current&&c->panels->mono_ready&&c->panels->mono_enabled){
  if(!c->clock_shown||changed||!c->panels->mono_overlay_enabled){aux_text_clock_render(&c->clock,c->panels);c->clock_shown=true;}
 }else c->clock_shown=false;
#endif
#else
 (void)c;(void)now;
#endif
}
