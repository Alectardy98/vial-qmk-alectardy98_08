/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_animation.h"
#include "my2k_usbh_chibios.h"
#include "main_oled_config.h"
#include "main_storage_mode.h"
#include "main_eeprom.h"
#include "eeprom_driver.h"
#include "my2k_oled_config.h"
#include "aux_link.h"
#include "timer.h"
#if MY2K_GAMES_PROFILE
#include "main_games.h"
#endif
#if MY2K_ACTIVE_DEV_PROFILE
#include "main_sleep.h"
#endif
#include <string.h>
#define API __attribute__((noinline,noclone,externally_visible))
static uint8_t state,error,wire_token,host_token,request[8],response[8],queued,saved;
static bool peer,restore,rate_peer,restore_rate,stop_test;
static uint8_t saved_rate,active_rate,saved_policy,active_policy,rate_version;
static uint8_t test_state[4];
static uint32_t sent,probe;
#define ADDRESS ((void *)(uintptr_t)32672u)
/* Own16bytes inside the existing reserved board-settings tail. Existing
 * ANI1, touch, LockLights and Vial macro/keymap addresses are unchanged. */
#define RATE_ADDRESS ((void *)(uintptr_t)32688u)
static void remember_rate(uint8_t rate,uint8_t policy){
 if((rate!=30&&rate!=60)||policy>1||my2k_eeprom32_reset_pending())return;
 uint8_t r[16]={2,rate,policy,0,'F','P','S','1'};uint32_t crc=my2k_oled_crc(r,12);
 for(unsigned i=0;i<4;i++)r[12+i]=(uint8_t)(crc>>(8*i));
 eeprom_update_block(r,RATE_ADDRESS,sizeof r);saved_rate=rate;saved_policy=policy;
}
static void remember(unsigned slot){
 if(!slot||slot>10||slot==saved||my2k_eeprom32_reset_pending())return;
 uint8_t record[16]={1,(uint8_t)slot,0,0,'A','N','I','1'};uint32_t crc=my2k_oled_crc(record,12);
 for(unsigned i=0;i<4;i++)record[12+i]=(uint8_t)(crc>>(8*i));
 eeprom_update_block(record,ADDRESS,sizeof record);saved=(uint8_t)slot;
}
API void my2k_animation_init(void){
 state=error=wire_token=host_token=queued=saved=active_rate=rate_version=saved_policy=0;active_policy=255;peer=rate_peer=false;restore=restore_rate=true;sent=probe=0;saved_rate=60;
 memset(test_state,0,sizeof test_state);
 uint8_t r[16];eeprom_read_block(r,ADDRESS,16);uint32_t crc=0;for(unsigned i=0;i<4;i++)crc|=(uint32_t)r[12+i]<<(8*i);
 if(r[0]==1&&r[1]>=1&&r[1]<=10&&!r[2]&&!r[3]&&!memcmp(r+4,"ANI1",4)&&!r[8]&&!r[9]&&!r[10]&&!r[11]&&my2k_oled_crc(r,12)==crc)saved=r[1];
 eeprom_read_block(r,RATE_ADDRESS,16);crc=0;for(unsigned i=0;i<4;i++)crc|=(uint32_t)r[12+i]<<(8*i);
 if((r[0]==1||r[0]==2)&&(r[1]==30||r[1]==60)&&(r[0]==1?!r[2]:r[2]<=1)&&!r[3]&&!memcmp(r+4,"FPS1",4)&&!r[8]&&!r[9]&&!r[10]&&!r[11]&&my2k_oled_crc(r,12)==crc){saved_rate=r[1];saved_policy=r[0]==1?1:r[2];}
}
static bool start(unsigned op,unsigned arg,unsigned part,unsigned token,uint32_t now){
 if(
#if MY2K_GAMES_PROFILE
 my2k_games_running()||
#endif
 state==1||my2k_usbh_animation_busy()||my2k_asset_export_active()||my2k_asset_transition_pending()||my2k_eeprom32_reset_pending()||my2k_oled_main_busy()||!my2k_aux_responsive())return false;
 my2k_wire_message m={.type=MY2K_AC,.v={(uint8_t)op,0,(uint8_t)arg,(uint8_t)part}};
 if(++wire_token==0)++wire_token;
 m.v[1]=wire_token;
 if(!my2k_aux_send(&m))return false;
 memcpy(request,m.v,8);host_token=token;state=1;error=0;sent=now;return true;
}
API void my2k_animation_key(uint8_t action){if((action>=1&&action<=10)||action==254||action==255){queued=action;stop_test=true;}}
API void my2k_animation_return(void){stop_test=true;restore=true;}
API void my2k_animation_receive(const my2k_wire_message *m){
 if(!m||m->type!=MY2K_AR||state!=1||memcmp(m->v,request,3))return;
 memcpy(response,m->v,8);state=m->v[3]?3:2;error=m->v[3];
 if(request[0]==8){rate_version=state==2&&!memcmp(response+4,"ANI5",4)?5:state==2&&!memcmp(response+4,"ANI4",4)?4:state==2&&!memcmp(response+4,"ANI3",4)?3:state==2&&!memcmp(response+4,"ANI2",4)?2:0;rate_peer=rate_version!=0;peer=rate_peer||(state==2&&!memcmp(response+4,"ANI1",4));}
 if(request[0]==9&&state==2&&request[2]){restore=false;remember(response[4]);}
 if(request[0]==11&&state==2){
  uint8_t policy=rate_version>=3?response[6]:1;
  if((response[4]!=30&&response[4]!=60)||policy>1||(request[2]&&(response[4]!=request[2]||(rate_version>=3&&policy!=request[3])))){state=3;error=3;return;}
  active_rate=response[4];active_policy=policy;if(host_token&&request[2])remember_rate(active_rate,active_policy);
 }
 if(request[0]==12&&state==2){
  if(response[4]>1||(request[2]<2&&response[4]!=request[2])){state=3;error=3;return;}
  memcpy(test_state,response+4,4);
 }
}
API void my2k_animation_service(uint32_t now){
 if(!my2k_aux_responsive()){peer=rate_peer=false;active_rate=rate_version=0;active_policy=255;restore=restore_rate=true;}
 if(state==1){if((uint32_t)(now-sent)>6000u){state=3;error=255;}return;}
 if(host_token){if((uint32_t)(now-sent)<10000u)return;host_token=0;}
 if(!peer){if((uint32_t)(now-probe)>=1000u){probe=now;(void)start(8,1,5,0,now);}return;}
 if(rate_peer&&restore_rate){if(start(11,saved_rate,rate_version>=3?saved_policy:0,0,now))restore_rate=false;return;}
 if(restore){
  /* ANI5 AUX holds its initial color frame until this explicit selection.
   * Slot0 accepts the discovered default when EEPROM has no saved slot. */
  if(rate_version>=5){if(start(9,saved,0,0,now))restore=false;return;}
  restore=false;if(saved)queued=saved;
 }
 if(stop_test){if(rate_version<4||start(12,0,0,0,now))stop_test=false;return;}
 if(queued&&start(9,queued,0,0,now))queued=0;
}
/* An available peer is not necessarily restored. Keep early host operations
 * from preempting the ACK-driven automatic rate/slot restore sequence. */
static bool host_ready(void){
 return peer&&(!rate_peer||!restore_rate)&&!restore&&!queued&&state!=1;
}
API bool my2k_animation_hid(uint8_t *r,unsigned n,bool writes){
 if(!r||n!=32||r[0]!=253||r[1]!=237)return false;
 uint8_t q[32];memcpy(q,r,32);memset(r,0,32);r[0]=253;r[1]=237;r[2]=85;r[3]=1;r[4]=q[2];r[5]=q[4];
 if(q[3]!=1||q[2]>2)return true;
 if(!q[2]){r[2]=170;memcpy(r+6,"ANI1",4);r[10]=peer;r[11]=10;r[12]=1;r[13]=rate_version>=4?7:rate_version==3?3:rate_peer;r[14]=saved_rate;r[15]=active_rate;r[16]=my2k_eeprom_pending();r[17]=my2k_eeprom_durable();r[18]=saved_policy;r[19]=active_policy;r[20]=rate_version;memcpy(r+21,test_state,4);r[25]=1;r[26]=host_ready();r[27]=state;r[28]=(restore?1u:0u)|(restore_rate&&rate_peer?2u:0u);r[29]=queued;return true;}
 if(q[2]==2){if(q[4]!=host_token)return true;r[2]=170;r[6]=state;r[7]=error;memcpy(r+8,response,8);if(state==2||state==3)host_token=0;return true;}
 unsigned op=q[5],arg=q[6],part=q[7];
 if(!q[4]||(op!=9&&op!=10&&op!=11&&op!=12)||(op==9&&arg>10&&arg!=254&&arg!=255)||(op==10&&(arg>10||part>3))||(op==9&&part)||!host_ready()||(op==9&&arg&&!writes)||
    (op==11&&(!rate_peer||part>(rate_version>=3?1u:0u)||(!arg&&part)||(arg&&arg!=30&&arg!=60)||(arg&&!writes)))||
    (op==12&&(rate_version<4||arg>2||part||(arg<2&&!writes))))return true;
 if(op>=11)for(unsigned i=8;i<32;i++)if(q[i])return true;
 if(start(op,arg,part,q[4],timer_read32())){
  r[2]=170;r[6]=1;
#if MY2K_ACTIVE_DEV_PROFILE
  if((op==9&&arg)||(op==11&&arg)||(op==12&&arg<2))my2k_sleep_activity(timer_read32());
#endif
 }
 return true;
}
