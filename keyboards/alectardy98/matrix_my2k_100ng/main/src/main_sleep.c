/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Visual idle policy only. Never suspends QMK, USB, matrix or storage workers. */
#pragma GCC optimize ("Os")
#include "main_sleep.h"
#include "my2k_oled_config.h"
#include "main_eeprom.h"
#include "main_persist.h"
#include "main_oled_config.h"
#include "eeprom_driver.h"
#include "aux_link.h"
#include "timer.h"
#include <string.h>
#define API __attribute__((noinline,noclone,externally_visible,optimize("Os")))
#define ADDRESS ((void *)(uintptr_t)32704u)
static uint8_t config[16], token, pending, argument, attempts, actual, fault;
static bool peer, wanted;
static uint32_t activity, sent, revision;
static uint32_t get(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static bool valid(const uint8_t*p){unsigned minutes=p[2]|(unsigned)p[3]<<8;return p[0]==1&&p[1]<=1&&minutes>=5&&minutes<=120&&!memcmp(p+4,"SLP1",4)&&!get(p+8)&&get(p+12)==my2k_oled_crc(p,12);}
API void my2k_sleep_init(uint32_t now){
 eeprom_read_block(config,ADDRESS,16);
 if(!valid(config)){memset(config,0,16);config[0]=1;config[2]=15;memcpy(config+4,"SLP1",4);put(config+12,my2k_oled_crc(config,12));}
 token=pending=attempts=fault=0;actual=255;peer=wanted=false;activity=now;sent=now-1000u;revision=1;
}
API void my2k_sleep_activity(uint32_t now){if(wanted&&fault){fault=0;attempts=0;}activity=now;wanted=false;}
API bool my2k_sleep_blank(void){return wanted&&peer&&actual==1&&!fault;}
API void my2k_sleep_receive(const my2k_wire_message*m){
 if(!m||m->type!=MY2K_AR||!pending||m->v[0]!=pending||m->v[1]!=token||m->v[2]!=argument)return;
 unsigned op=pending;pending=0;
 if(m->v[3]){if(m->v[3]==2&&attempts<3)return;fault=m->v[3];return;}
 if(op==13){peer=!memcmp(m->v+4,"SLP1",4);fault=peer?0:1;actual=255;}
 else if(m->v[4]==argument&&m->v[5]==3){actual=argument;fault=0;attempts=0;}
 else fault=3;
}
API void my2k_sleep_service(uint32_t now,bool inhibited){
 unsigned minutes=config[2]|(unsigned)config[3]<<8;
 wanted=config[1]&&!inhibited&&(uint32_t)(now-activity)>=(uint32_t)minutes*60000u;
 if(!my2k_aux_responsive()){peer=false;actual=255;pending=attempts=fault=0;return;}
 if(!peer&&!my2k_oled_main_ready())return;
 if(pending){if((uint32_t)(now-sent)<1000u)return;pending=0;if(attempts>=3){fault=255;return;}}
 if(fault||(!peer&&attempts>=3))return;
 if(peer&&actual==(unsigned)wanted){attempts=0;return;}
 if((!peer||wanted)&&(uint32_t)(now-sent)<250u)return;
 my2k_wire_message m={.type=MY2K_AC,.v={peer?14:13,0,peer?(uint8_t)wanted:0,1}};
 if(++token==0)++token;
 m.v[1]=token;
 if(my2k_aux_send(&m)){pending=m.v[0];argument=m.v[2];sent=now;attempts++;}
}
API bool my2k_sleep_hid(uint8_t*p,unsigned n,bool writes){
 if(!p||n!=32||p[0]!=253||p[1]!=231)return false;
 uint8_t q[32];memcpy(q,p,32);memset(p,0,32);p[0]=253;p[1]=231;p[2]=85;p[3]=1;p[4]=q[2];p[5]=q[4];
 #define FAIL(e) do{p[6]=(e);return true;}while(0)
 unsigned op=q[2];if(q[3]!=1||op>3)FAIL(1);
 unsigned end=op==2?25:5;for(unsigned i=end;i<32;i++)if(q[i])FAIL(1);
 if(op==0){memcpy(p+6,"SLP1",4);p[10]=16;p[11]=5;p[12]=120;p[13]=peer;p[14]=fault;p[15]=1;}
 if(op==2){
  if(!writes||!peer)FAIL(2);
  if(!q[4]||get(q+5)!=revision)FAIL(3);
  if(!valid(q+9))FAIL(4);
  if(!my2k_eeprom_durable()||my2k_persist_error())FAIL(5);
  eeprom_update_block(q+9,ADDRESS,16);uint8_t r[16];eeprom_read_block(r,ADDRESS,16);
  if(memcmp(r,q+9,16)||my2k_persist_error())FAIL(5);
  memcpy(config,r,16);revision++;my2k_sleep_activity(timer_read32());
 }
 if(op==1){memcpy(p+8,config,16);put(p+24,revision);}
 if(op==0||op==2||op==3){put(p+16,revision);p[20]=my2k_eeprom_pending();put(p+24,my2k_persist_error());}
 if(op==3){p[6]=wanted;p[7]=actual;p[8]=peer;p[9]=pending;p[10]=fault;p[11]=attempts;put(p+12,(uint32_t)(timer_read32()-activity));}
 p[2]=170;return true;
 #undef FAIL
}
