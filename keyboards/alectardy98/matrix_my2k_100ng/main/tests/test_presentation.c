/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_animation.h"
#include "main_games.h"
#include "my2k_oled_config.h"
#include "host_driver.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t eeprom[32768];
static uint32_t now;
static my2k_wire_message sent;
static unsigned sends;
static bool high_speed;
static bool aux_responsive = true, oled_busy;
int vial_unlocked=1;
uint32_t timer_read32(void){return now;}
void eeprom_read_block(void *dst,const void *src,size_t n){assert((uintptr_t)src+n<=sizeof eeprom);memcpy(dst,eeprom+(uintptr_t)src,n);}
void eeprom_update_block(const void *src,void *dst,size_t n){assert((uintptr_t)dst+n<=sizeof eeprom);memcpy(eeprom+(uintptr_t)dst,src,n);}
bool my2k_aux_send(const my2k_wire_message *m){sent=*m;sends++;return true;}
bool my2k_aux_responsive(void){return aux_responsive;}
bool my2k_usbh_animation_busy(void){return false;}
bool my2k_asset_export_active(void){return false;}
bool my2k_asset_transition_pending(void){return false;}
bool my2k_asset_mode_request_key(uint8_t r,uint8_t c,bool p,uint32_t t){(void)r;(void)c;(void)p;(void)t;return true;}
bool my2k_eeprom32_reset_pending(void){return false;}
bool my2k_eeprom_durable(void){return true;}
bool my2k_eeprom_pending(void){return false;}
uint32_t my2k_persist_error(void){return 0;}
bool my2k_oled_main_busy(void){return oled_busy;}
bool my2k_oled_main_ready(void){return true;}
bool my2k_performance_hs_requested(void){return high_speed;}
bool my2k_performance_mode_changing(void){return false;}
void clear_keyboard(void){}
void my2k_touchbar_cancel(void){}
bool my2k_touchbar_preserve_legacy(void){return true;}
void my2k_sleep_activity(uint32_t n){(void)n;}
host_driver_t *host_get_driver(void){return NULL;}
void host_set_driver(host_driver_t *d){(void)d;}
static my2k_wire_message ack(unsigned a,unsigned b,unsigned c,unsigned d){my2k_wire_message r={.type=MY2K_AR};memcpy(r.v,sent.v,3);r.v[4]=a;r.v[5]=b;r.v[6]=c;r.v[7]=d;return r;}
static void record(unsigned slot){uint8_t *p=eeprom+32672;memset(p,0,16);p[0]=1;p[1]=slot;memcpy(p+4,"ANI1",4);uint32_t crc=my2k_oled_crc(p,12);for(unsigned i=0;i<4;i++)p[12+i]=(uint8_t)(crc>>(8*i));}
static void startup_probe(bool wrap) {
 my2k_animation_init();
 now = wrap ? UINT32_MAX - 50u : 0;
 aux_responsive = false;
 my2k_animation_service(now);
 assert(sends == 0);
 aux_responsive = true;
 my2k_animation_service(now);
 assert(sends == 1 && sent.v[0] == 8); /* No artificial initial one-second wait. */
 my2k_animation_service(now + 50u);
 assert(sends == 1); /* Never duplicate an in-flight command. */
 my2k_wire_message r = ack(0,0,0,0);
 r.v[3] = 2;
 my2k_animation_receive(&r);
 my2k_animation_service(now + 99u);
 assert(sends == 1);
 oled_busy = true;
 my2k_animation_service(now + 100u);
 assert(sends == 1); /* Local deferral must not restart the remote retry timer. */
 oled_busy = false;
 my2k_animation_service(now + 101u);
 assert(sends == 2 && sent.v[0] == 8);
 now += 101u;
 r = ack(0,0,0,0);
 r.v[3] = 1; /* Unsupported peer retains the normal backoff. */
 my2k_animation_receive(&r);
 my2k_animation_service(now + 999u);
 assert(sends == 2);
 my2k_animation_service(now + 1000u);
 assert(sends == 3);
 puts("PASS immediate startup probe, bounded busy retry, OLED admission, error backoff and timer wrap");
}
static void animation(unsigned slot,unsigned version){
 if(slot)record(slot);
 my2k_animation_init();now=1000;my2k_animation_service(now);
 assert(sent.v[0]==8&&sent.v[2]==1&&sent.v[3]==5);
 my2k_wire_message r=ack('A','N','I','0'+version);my2k_animation_receive(&r);
 my2k_animation_service(++now);assert(sent.v[0]==11);r=ack(60,60,0,0);my2k_animation_receive(&r);
 my2k_animation_service(++now);assert(sent.v[0]==9&&sent.v[2]==slot);
 r=ack(slot,0,0,0);my2k_animation_receive(&r);
 uint8_t q[32]={253,237,0,1};assert(my2k_animation_hid(q,32,false)&&q[2]==170&&q[26]);
 puts("PASS Main saved/default startup selection and ACK-gated readiness");
}
static void mode(unsigned target){uint8_t q[32]={253,230,2,1,target};assert(my2k_games_hid(q,32)&&q[2]==170);}
static void games(bool atomic){
 now=1000;my2k_games_service(now);assert(sent.v[0]==15);my2k_wire_message r=ack('G','A','M','3');my2k_games_receive(&r);
 my2k_games_service(++now);assert(sent.v[0]==16&&sent.v[2]==2&&sent.v[3]==1);r=ack(0,0,atomic?3:1,0);my2k_games_receive(&r);
 const unsigned targets[]={1,3,2,1,0},wire_targets[]={1,4,3,1,0};
 unsigned active=0;
 for(unsigned i=0;i<5;i++){
  mode(targets[i]);now+=100;unsigned before=sends;my2k_games_service(now);assert(sends==before+1&&sent.v[0]==16);
  if(!atomic&&active){assert(sent.v[2]==0&&sent.v[3]==1);r=ack(0,0,1,0);my2k_games_receive(&r);active=0;
   if(targets[i]){now+=100;my2k_games_service(now);}
  }
  assert(sent.v[2]==wire_targets[i]&&sent.v[3]==(atomic?2:1));
  r=ack(targets[i],0,atomic?3:1,0);my2k_games_receive(&r);active=targets[i];
 }
 assert(!my2k_games_running());
 high_speed=true;mode(1);now+=100;unsigned before=sends;my2k_games_service(now);assert(sends==before&&!my2k_games_running());
 puts("PASS Main direct NES/GBC/Doom handoff, legacy fallback, normal return and 8K game gate");
}
int main(int argc,char **argv){assert(argc==2);if(!strcmp(argv[1],"saved"))animation(2,5);else if(!strcmp(argv[1],"default"))animation(0,5);else if(!strcmp(argv[1],"older-aux"))animation(2,4);else if(!strcmp(argv[1],"highspeed")){high_speed=true;animation(2,5);}else if(!strcmp(argv[1],"probe")||!strcmp(argv[1],"probe-wrap"))startup_probe(!strcmp(argv[1],"probe-wrap"));else games(!strcmp(argv[1],"atomic"));}
