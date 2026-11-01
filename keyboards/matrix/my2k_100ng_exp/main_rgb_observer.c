/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_rgb_observer.h"
#include <string.h>
#if !MY2K_RGB_OBSERVER_HOST
#include "main_rgb_bus.h"
#include "main_perf.h"
#define my2k_rgb_observer_write my2k_rgb_bus_write
#define my2k_rgb_observer_read my2k_rgb_bus_read
#define my2k_rgb_observer_clock my2k_perf_clock
#endif
#define SETS 8u
#define SLOTS (SETS*3u)
typedef struct {
 uint32_t started,finished,event_start,event_end;
 uint16_t code;
 uint8_t row,col,chip,valid,fn[13],control[18],pwm[3],expected_pwm[3],expected_control[18];
#if MY2K_DEV18_PROFILE
 uint8_t fn_b[13],control_b[18],pwm_b[3],fn_c[13],control_c[18],pwm_c[3];
#endif
} snapshot;
static snapshot records[SLOTS];
static uint32_t events,bank_writes,reads,errors,max_step_us,last_step;
static uint8_t key_row,key_col,sets,chip_offset,phase,used,unsafe;
static uint16_t key_code;
static bool requested,busy,stopped;
#if MY2K_DEV18_PROFILE
static uint8_t subphase;
static uint32_t set_finished;
#endif
static bool select_bank(unsigned chip,uint8_t bank){
 bank_writes++;
 return my2k_rgb_observer_write(my2k_rgb_drivers[chip].address_byte,0xfd,&bank,1,5)==0;
}
bool my2k_rgb_observer_bank_safe(unsigned chip){return chip<6&&!(unsafe&(1u<<chip));}
void my2k_rgb_observer_key(uint8_t row,uint8_t col,uint16_t code,bool pressed){
 if(!pressed||row>=8||col>=16)return;
 events++;key_row=row;key_col=col;key_code=code;
 if(sets<SETS)requested=true;
}
static void expected(snapshot *s){
 for(unsigned led=66;led<125;led++){
  const my2k_rgb_channel *c=&my2k_rgb_channels[led];if(c->driver!=s->chip)continue;
  uint8_t channels[]={c->r,c->g,c->b};
  for(unsigned j=0;j<3;j++)s->expected_control[channels[j]/8]|=1u<<(channels[j]%8);
 }
}
__attribute__((noipa,externally_visible))
void my2k_rgb_observer_service(const my2k_rgb_state *state,uint32_t now){
 if(!state||stopped||sets>=SETS||(uint32_t)(now-last_step)<5u)return;
 if(!busy){
  /* One baseline after startup settles, then at most seven requested captures.
   * GETs do not arm it; a key hook sets RAM intent, never accesses the bus. */
#if MY2K_DEV18_PROFILE
  bool quiet_repeat=sets<3&&!events;
  if((!sets&&now<1000u)||(sets&&!requested&&!quiet_repeat))return;
  if(sets&&quiet_repeat&&(uint32_t)(now-set_finished)<250u)return;
  subphase=0;
#else
  if((!sets&&now<1000u)||(sets&&!requested))return;
#endif
  busy=true;requested=false;chip_offset=phase=0;
 }
 last_step=now;
 snapshot *s=&records[sets*3u+chip_offset];unsigned chip=chip_offset+2;
 if(!phase
#if MY2K_DEV18_PROFILE
    &&!subphase
#endif
 ) {
  memset(s,0,sizeof *s);s->chip=chip;s->started=now;s->event_start=events;
  s->row=key_row;s->col=key_col;s->code=key_code;expected(s);
 }
 uint32_t started=my2k_rgb_observer_clock();uint8_t bank=phase<5?0x0b:0,reg,n,*out;
 if(phase<5){reg=phase*3;n=13-reg;if(n>3)n=3;out=s->fn+reg;}
 else if(phase<11){reg=(phase-5)*3;n=3;out=s->control+reg;}
 else{
  unsigned led=chip==2?66:chip==3?85:104,color=phase-11;
  const my2k_rgb_channel *c=&my2k_rgb_channels[led];uint8_t channels[]={c->r,c->g,c->b};
  reg=0x24+channels[color];n=1;out=s->pwm+color;
  const my2k_rgb_color *v=&state->rendered[led];s->expected_pwm[color]=color==0?v->r:color==1?v->g:v->b;
 }
#if MY2K_DEV18_PROFILE
 uint8_t segment_n=n;
 if(subphase){
  unsigned offset=(subphase-1)/2,repeat=(subphase-1)%2;
  if(phase<5)out=(repeat?s->fn_c:s->fn_b)+reg+offset;
  else if(phase<11)out=(repeat?s->control_c:s->control_b)+reg+offset;
  else out=(repeat?s->pwm_c:s->pwm_b)+(phase-11);
  reg+=offset;n=1;
 }
#endif
 /* Keep page select, read, cleanup in one task invocation. No scheduler yield
  * or nested keyboard_task while the bus/page is owned. Read3 bytes maximum. */
 bool ok=select_bank(chip,bank);
 if(ok){reads++;ok=my2k_rgb_observer_read(my2k_rgb_drivers[chip].address_byte,reg,out,n,5)==0;}
 bool restored=select_bank(chip,0);
 if(!restored)unsafe|=1u<<chip;
 else unsafe&=~(1u<<chip);
 uint32_t us=(my2k_rgb_observer_clock()-started)/168u;if(us>max_step_us)max_step_us=us;
 if(!ok||!restored){
  errors++;s->valid=restored?2:4;s->finished=now;s->event_end=events;
  used=sets*3u+chip_offset+1;stopped=true;busy=false;return;
 }
#if MY2K_DEV18_PROFILE
 if(++subphase<1u+2u*segment_n)return;
 subphase=0;
#endif
 if(++phase==14){
  s->finished=now;s->event_end=events;s->valid=1;
#if MY2K_DEV18_PROFILE
  if(!memcmp(s->fn,s->fn_b,13)&&!memcmp(s->fn,s->fn_c,13)&&!memcmp(s->control,s->control_b,18)&&!memcmp(s->control,s->control_c,18)&&!memcmp(s->pwm,s->pwm_b,3)&&!memcmp(s->pwm,s->pwm_c,3))s->valid|=8;
#endif
  used=sets*3u+chip_offset+1;
  phase=0;if(++chip_offset==3){sets++;busy=false;
#if MY2K_DEV18_PROFILE
   set_finished=now;
#endif
  }
 }
}
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
__attribute__((noipa,externally_visible))
bool my2k_rgb_observer_page(uint8_t page,uint8_t index,uint8_t p[32]){
 if(!p||page<31||page>(
#if MY2K_DEV18_PROFILE
 40
#else
 35
#endif
 )||(page==31?index!=0:index>=used))return false;
 memset(p+4,0,28);p[4]=
#if MY2K_DEV18_PROFILE
 2;
#else
 1;
#endif
 if(page==31){
  p[5]=used;p[6]=SLOTS;p[7]=(busy?1:0)|(stopped?2:0)|(sets>=SETS?4:0);
  put(p+8,sets);put(p+12,events);put(p+16,bank_writes);put(p+20,reads);put(p+24,errors);put(p+28,max_step_us);return true;
 }
 const snapshot *s=&records[index];p[5]=index;p[6]=s->chip;p[7]=s->valid;
 if(page==32){put(p+8,s->started);put(p+12,s->finished);put(p+16,s->event_start);put(p+20,s->event_end);p[24]=s->row;p[25]=s->col;p[26]=s->code;p[27]=s->code>>8;}
 else if(page==33){memcpy(p+8,s->fn,13);memcpy(p+21,s->pwm,3);memcpy(p+24,s->expected_pwm,3);}
#if MY2K_DEV18_PROFILE
 else if(page==36||page==38){memcpy(p+8,page==36?s->fn_b:s->fn_c,13);memcpy(p+21,page==36?s->pwm_b:s->pwm_c,3);}
 else if(page==37||page==39)memcpy(p+8,page==37?s->control_b:s->control_c,18);
 else if(page==40){
  uint64_t mask=0;
  for(unsigned i=0;i<13;i++)if(s->fn[i]!=s->fn_b[i]||s->fn[i]!=s->fn_c[i])mask|=(uint64_t)1<<i;
  for(unsigned i=0;i<18;i++)if(s->control[i]!=s->control_b[i]||s->control[i]!=s->control_c[i])mask|=(uint64_t)1<<(i+13);
  for(unsigned i=0;i<3;i++)if(s->pwm[i]!=s->pwm_b[i]||s->pwm[i]!=s->pwm_c[i])mask|=(uint64_t)1<<(i+31);
  put(p+8,(uint32_t)mask);put(p+12,(uint32_t)(mask>>32));
 }
#endif
 else memcpy(p+8,page==34?s->control:s->expected_control,18);
 return true;
}
