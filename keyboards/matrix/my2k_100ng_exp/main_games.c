/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_games.h"
#include "main_keycodes.h"
#include "main_performance_mode.h"
#include "main_storage_mode.h"
#include "main_sleep.h"
#include "main_oled_config.h"
#include "aux_link.h"
#include "main_eeprom.h"
#include "main_persist.h"
#include "eeprom_driver.h"
#include "my2k_oled_config.h"
#include "main_animation.h"
#include "vial.h"
#include "main_touch_config.h"
#include "report.h"
#include "host.h"
#include "host_driver.h"
static void filter_install(void);
#include <string.h>

#define API __attribute__((noinline,noclone,externally_visible))
static uint8_t peer,active,error,pending,token,wanted,buttons,sent_buttons;
/* Last48 bytes of the already-reserved board tail; no Vial/OLED address moves. */
#define MAP_ADDRESS ((void *)(uintptr_t)32720u)
static uint8_t mapping[48];
static bool mapping_loaded;
/* Obsolete touch-v1 slot, after explicit migration of any legacy touch data. */
#define GB_MAP_ADDRESS ((void *)(uintptr_t)32640u)
static uint8_t gb_mapping[16];
static bool gb_loaded;
static uint32_t mapping_revision=1;
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static bool mapping_key(unsigned c){return !c||(c>=4&&c<=0x73)||(c>=0x87&&c<=0xa4)||(c>=0xe0&&c<=0xe7);}
static bool mapping_valid(const uint8_t *p){
 if(memcmp(p,"GMT1",4)||p[4]!=1||p[5]!=2||p[6]!=8||p[7]||get32(p+40)||get32(p+44)!=my2k_oled_crc(p,44))return false;
 for(unsigned i=8;i<40;i+=2)if(!mapping_key(p[i]|(unsigned)p[i+1]<<8))return false;
 return true;
}
static void mapping_init(void){
 if(!gb_loaded){
  gb_loaded=true;eeprom_read_block(gb_mapping,GB_MAP_ADDRESS,16);
  bool valid=!memcmp(gb_mapping,"GBM1",4)&&get32(gb_mapping+12)==my2k_oled_crc(gb_mapping,12);
  for(unsigned i=4;i<12;i++)if(!mapping_key(gb_mapping[i]))valid=false;
  if(!valid){const uint8_t d[8]={KC_Z,KC_X,KC_RSFT,KC_ENTER,KC_UP,KC_DOWN,KC_LEFT,KC_RIGHT};memcpy(gb_mapping,"GBM1",4);memcpy(gb_mapping+4,d,8);put32(gb_mapping+12,my2k_oled_crc(gb_mapping,12));}
 }
 if(mapping_loaded)return;
 mapping_loaded=true;eeprom_read_block(mapping,MAP_ADDRESS,sizeof mapping);
 if(mapping_valid(mapping))return;
 memset(mapping,0,sizeof mapping);memcpy(mapping,"GMT1",4);mapping[4]=1;mapping[5]=2;mapping[6]=8;
 const uint8_t defaults[16]={KC_Z,KC_X,KC_RSFT,KC_ENTER,KC_UP,KC_DOWN,KC_LEFT,KC_RIGHT,KC_Z,KC_X,KC_RSFT,KC_ENTER,KC_W,KC_S,KC_A,KC_D};
 for(unsigned i=0;i<16;i++)mapping[8+2*i]=defaults[i];
 put32(mapping+44,my2k_oled_crc(mapping,44));
}
static uint8_t physical_buttons[MATRIX_ROWS*MATRIX_COLS],virtual_buttons;
static void refresh_buttons(void){
 buttons=virtual_buttons;
 for(unsigned i=0;i<sizeof physical_buttons;i++)buttons|=physical_buttons[i];
}
static void clear_inputs(void){
 memset(physical_buttons,0,sizeof physical_buttons);virtual_buttons=buttons=sent_buttons=0;
}
static uint32_t at,attempt_at;
static uint8_t wire[8],stats[20],part,export_row,export_col;
static bool export_pending,status_read,synced;
static bool send(unsigned op,unsigned arg,uint32_t now){
 my2k_wire_message m={.type=MY2K_AC,.v={(uint8_t)op,0,(uint8_t)arg,1}};
 if(++token==0)++token;
 m.v[1]=token;
 if(!my2k_aux_send(&m))return false;
 memcpy(wire,m.v,8);pending=op;at=now;return true;
}
static void select_mode(unsigned mode){
 if(mode>3||(mode==2&&peer<2)||(mode==3&&peer<3)){error=7;return;} /* GAM2 explicitly negotiates Doom. */
 if(mode&&(!peer||!synced)){error=9;return;} /* Reject without consuming normal keys on an older/unready AUX. */
 if(mode&&(my2k_performance_hs_requested()||my2k_performance_mode_changing()||my2k_asset_export_active()||my2k_asset_transition_pending())){error=6;return;}
 clear_keyboard();my2k_touchbar_cancel();
 wanted=(uint8_t)mode;attempt_at=timer_read32();error=0;
 my2k_sleep_activity(attempt_at);
}
API void my2k_games_receive(const my2k_wire_message *m){
 if(!pending||m->type!=MY2K_AR||memcmp(m->v,wire,3))return;
 unsigned op=pending;pending=0;error=m->v[3];
 if(op==15){peer=error?0:!memcmp(m->v+4,"GAM3",4)?3:!memcmp(m->v+4,"GAM2",4)?2:!memcmp(m->v+4,"NES1",4)?1:0;return;}
 if(error)return;
 if(op==16){
  uint8_t previous=active;bool changed=active!=m->v[4];active=m->v[4];error=m->v[5];if(changed){clear_inputs();clear_keyboard();}
  if(wire[2]==2){if(!synced)attempt_at=at;synced=true;status_read=true;if(!active){if(wanted==previous)wanted=0;error=m->v[5];}}
 }
 if(op==17)sent_buttons=wire[2];
 if(op==18){memcpy(stats+4*wire[2],m->v+4,4);part=(wire[2]+1u)%5u;if(!part)status_read=false;}
}
API void my2k_games_service(uint32_t now){
 mapping_init();filter_install();
 if(!my2k_aux_responsive()){peer=active=pending=wanted=0;export_pending=synced=false;clear_inputs();return;}
 if(pending){if((uint32_t)(now-at)>(pending==16&&wire[2]==3?15000u:3000u)){pending=0;error=255;wanted=active;}return;}
 if(my2k_asset_export_active()||my2k_asset_transition_pending()){active=wanted=0;return;}
 if(!my2k_oled_main_ready())return;
 if(!peer){if((uint32_t)(now-at)>=1000u)(void)send(15,0,now);return;}
 if(!synced){(void)send(16,2,now);return;}
 if(wanted!=active){
  if((uint32_t)(now-attempt_at)>(wanted==2?15000u:2500u)){wanted=active;return;}
  if(!error||error==2){if((uint32_t)(now-at)>=20u)(void)send(16,active?0:wanted==3?4:wanted==2?3:wanted,now);}else wanted=active;
  return;
 }
 if(!active&&export_pending){export_pending=false;if(my2k_asset_mode_request_key(export_row,export_col,true,now))clear_keyboard();else error=8;return;}
 if(active&&buttons!=sent_buttons){(void)send(17,buttons,now);return;}
 if(active&&(uint32_t)(now-at)>=100u)(void)send(!part&&!status_read?16:18,!part&&!status_read?2:part,now);
}
API bool my2k_games_key(uint16_t code,keyrecord_t *r){
 mapping_init();
 if(code==MY2K_OLED_MODE_TOGGLE){
  if(r->event.pressed){
   /* Cycle requested state, so a second press during loading advances once
    * rather than replaying the last command. Existing service stops before
    * changing games; storage/High-speed/peer gates remain in select_mode. */
   unsigned next=wanted==0?1:wanted==1?3:wanted==3?2:0;
   select_mode(next);
   if(!next)my2k_animation_return();
  }
  return false;
 }
 if(code==MY2K_NORMAL_MODE||code==MY2K_NES_TOGGLE||code==MY2K_DOOM_TOGGLE||code==MY2K_GBC_TOGGLE){
  if(r->event.pressed){
   unsigned target=code==MY2K_NORMAL_MODE?0:code==MY2K_NES_TOGGLE?1:code==MY2K_GBC_TOGGLE?3:2;
   select_mode(target&&(wanted==target||active==target)?0:target);
   if(!target)my2k_animation_return();
  }
  return false;
 }
 if(code==MY2K_ANIMATION_NEXT){
  if(r->event.pressed){if(active||wanted)select_mode(0);my2k_animation_key(255);}
  return false;
 }
 if(!active&&!wanted)return true;
 my2k_sleep_activity(timer_read32());
 /* Release by physical identity even if a layer/configuration changed meanwhile. */
 bool physical=r->event.key.row<MATRIX_ROWS&&r->event.key.col<MATRIX_COLS;
 unsigned position=(unsigned)r->event.key.row*MATRIX_COLS+r->event.key.col;
 if(physical&&!r->event.pressed){physical_buttons[position]=0;refresh_buttons();}
 /* Pure layer controls retain QMK press/release semantics, including a
  * momentary layer held before entering a game. They emit no typing report. */
 if(IS_QK_TO(code)||IS_QK_MOMENTARY(code)||IS_QK_DEF_LAYER(code)||IS_QK_TOGGLE_LAYER(code)||IS_QK_ONE_SHOT_LAYER(code)||IS_QK_LAYER_TAP_TOGGLE(code)||code==QK_BOOTLOADER)return true;
 if(code==MY2K_STORAGE_TOGGLE){if(r->event.pressed){if(code==MY2K_STORAGE_TOGGLE){export_pending=true;export_row=r->event.key.row;export_col=r->event.key.col;}select_mode(0);}return false;}
 /* Game mappings match resolved ordinary QMK keycodes, independent of matrix position. */
 uint8_t bit=0;unsigned mode=wanted?wanted:active;
 for(unsigned i=0;i<8;i++){unsigned at=8+(mode-1u)*16+2*i;unsigned binding=mode==3?gb_mapping[4+i]:mapping[at]|(unsigned)mapping[at+1]<<8;if(binding&&code==binding)bit|=(uint8_t)(1u<<i);}
 if(physical&&r->event.pressed){physical_buttons[position]=bit;refresh_buttons();}
 return false;
}
API bool my2k_games_hid(uint8_t *p,unsigned n){
 if(n!=32||p[0]!=253||p[1]!=230)return false;
 mapping_init();
 uint8_t op=p[2],arg=p[4];
 if(op==5||op==6){
  uint8_t q[32];memcpy(q,p,32);memset(p,0,32);p[0]=253;p[1]=230;p[2]=85;p[3]=1;p[4]=op;p[5]=arg;
  if(q[3]!=1||arg<1||arg>3)return true;
  unsigned end=op==6?25:5;for(unsigned i=end;i<32;i++)if(q[i])return true;
  if(op==6){
   if(!vial_unlocked||active||wanted||my2k_asset_export_active()||my2k_asset_transition_pending()||my2k_eeprom32_reset_pending()){p[6]=2;return true;}
   if(get32(q+5)!=mapping_revision){p[6]=3;return true;}
   for(unsigned i=9;i<25;i+=2)if(!mapping_key(q[i]|(unsigned)q[i+1]<<8)){p[6]=4;return true;}
   if(!my2k_eeprom_durable()||my2k_persist_error()){p[6]=5;return true;}
   if(arg==3){
    uint8_t next[16];memcpy(next,"GBM1",4);for(unsigned i=0;i<8;i++)next[4+i]=q[9+2*i];put32(next+12,my2k_oled_crc(next,12));
    if(!my2k_touchbar_preserve_legacy()){p[6]=5;return true;}
    eeprom_update_block(next,GB_MAP_ADDRESS,16);uint8_t saved[16];eeprom_read_block(saved,GB_MAP_ADDRESS,16);
    if(memcmp(saved,next,16)||my2k_persist_error()){p[6]=5;return true;}
    memcpy(gb_mapping,next,16);mapping_revision++;
   }else{
   uint8_t next[48];memcpy(next,mapping,48);memcpy(next+8+(arg-1u)*16,q+9,16);put32(next+44,my2k_oled_crc(next,44));
   eeprom_update_block(next,MAP_ADDRESS,48);uint8_t saved[48];eeprom_read_block(saved,MAP_ADDRESS,48);
   if(memcmp(saved,next,48)||my2k_persist_error()){p[6]=5;return true;}
   memcpy(mapping,next,48);mapping_revision++;
   }
  }
  p[2]=170;if(arg==3){for(unsigned i=0;i<8;i++){p[8+2*i]=gb_mapping[4+i];p[9+2*i]=0;}}else memcpy(p+8,mapping+8+(arg-1u)*16,16);put32(p+24,mapping_revision);p[28]=my2k_eeprom_pending();p[29]=my2k_persist_error()!=0;return true;
 }
 bool valid=p[3]==1;
 for(unsigned i=5;i<32;i++)if(p[i])valid=false;
 memset(p,0,32);p[0]=253;p[1]=230;p[2]=85;p[3]=1;p[4]=op;
 if(!valid||op>4||(op!=2&&op!=3&&arg))return true;
 if(op==2){if(arg>3||(arg==2&&peer<2)||(arg==3&&peer<3)||!peer||!synced)return true;select_mode(arg);}
 if(op==3){if(!active)return true;virtual_buttons=arg;refresh_buttons();my2k_sleep_activity(timer_read32());}
 if(op==0){memcpy(p+8,"GME1",4);p[28]=(peer>=3?31:peer==2?15:11);}else if(op==4)memcpy(p+8,stats+16,4);else memcpy(p+8,stats,16);
 p[2]=170;p[5]=peer&&synced;p[6]=active;p[7]=error;p[24]=pending;p[25]=wanted;p[26]=buttons;p[27]=my2k_performance_hs_requested();p[29]=sent_buttons;return true;
}

API bool my2k_games_running(void){return active||wanted||(peer&&!synced);}

/* Final board-local output fence: catches deferred macros/tap-dance/mouse/touch,
 * beyond the physical-key hook. Neutral releases and raw Vial HID remain live.
 * No USB descriptors, endpoint intervals or shared source are changed. */
static host_driver_t original_driver,game_driver;
API void my2k_games_send_keyboard(report_keyboard_t *r){
 if(my2k_games_running()){report_keyboard_t n=*r;n.mods=0;n.reserved=0;memset(n.keys,0,sizeof n.keys);original_driver.send_keyboard(&n);}else original_driver.send_keyboard(r);
}
API void my2k_games_send_nkro(report_nkro_t *r){
 if(my2k_games_running()){report_nkro_t n=*r;n.mods=0;memset(n.bits,0,sizeof n.bits);original_driver.send_nkro(&n);}else original_driver.send_nkro(r);
}
API void my2k_games_send_mouse(report_mouse_t *r){
 if(my2k_games_running()){report_mouse_t n=*r;n.buttons=0;n.x=n.y=n.v=n.h=0;
#ifdef MOUSE_EXTENDED_REPORT
 n.boot_x=n.boot_y=0;
#endif
 original_driver.send_mouse(&n);}else original_driver.send_mouse(r);
}
API void my2k_games_send_extra(report_extra_t *r){
 if(my2k_games_running()){report_extra_t n=*r;n.usage=0;original_driver.send_extra(&n);}else original_driver.send_extra(r);
}

static void filter_install(void){
 host_driver_t *d=host_get_driver();if(!d||d==&game_driver)return;
 original_driver=*d;game_driver=*d;
 if(d->send_keyboard)game_driver.send_keyboard=my2k_games_send_keyboard;
 if(d->send_nkro)game_driver.send_nkro=my2k_games_send_nkro;
 if(d->send_mouse)game_driver.send_mouse=my2k_games_send_mouse;
 if(d->send_extra)game_driver.send_extra=my2k_games_send_extra;
 host_set_driver(&game_driver);
}
