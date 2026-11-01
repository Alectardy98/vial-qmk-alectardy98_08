/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_rgb_refresh.h"
#include <string.h>
static uint8_t desired[6][144],sent[6][144],cursor[6],next_chip;
static uint8_t uncertain[6][144];
static bool latch,retrying[6],was_pending;
static uint32_t retry_at[6];
static my2k_refresh_stats stats;
static unsigned size(unsigned chip){return chip==5?36:144;}
static bool usable(unsigned chip,unsigned c){return chip>=2 || (c<143 && (c&15)!=15);}
void my2k_refresh_init(void){
 memset(desired,0,sizeof desired);memset(sent,0,sizeof sent);memset(cursor,0,sizeof cursor);memset(uncertain,0,sizeof uncertain);
 memset(retrying,0,sizeof retrying);memset(retry_at,0,sizeof retry_at);
 next_chip=0;latch=was_pending=false;memset(&stats,0,sizeof stats);
}
void my2k_refresh_set(unsigned chip,unsigned channel,uint8_t value){if(chip<6 && channel<size(chip) && usable(chip,channel))desired[chip][channel]=value;}
bool my2k_refresh_pending(void){if(latch || memcmp(desired,sent,sizeof desired)!=0)return true;for(unsigned i=0;i<sizeof uncertain;i++)if(((uint8_t*)uncertain)[i])return true;return false;}
void my2k_refresh_staged(void){stats.staged++;if(was_pending && my2k_refresh_pending())stats.coalesced++;was_pending=my2k_refresh_pending();}
my2k_refresh_stats my2k_refresh_diagnostics(void){return stats;}
bool my2k_refresh_step(uint8_t allowed,uint32_t now,my2k_refresh_write write){
 for(unsigned n=0;n<6;n++){
  unsigned chip=(next_chip+n)%6,limit=size(chip);if(!(allowed&(1u<<chip)))continue;
  if(retrying[chip] && (int32_t)(now-retry_at[chip])<0)continue;
  unsigned first=limit;
  for(unsigned i=0;i<limit;i++){unsigned c=(cursor[chip]+i)%limit;if(usable(chip,c) && (uncertain[chip][c] || desired[chip][c]!=sent[chip][c])){first=c;break;}}
  bool commit=chip==5 && first==limit && latch;
  if(first==limit && !commit)continue;
  unsigned count=commit?1:limit-first;if(count>16)count=16;
  if(chip<2 && count>15-(first&15))count=15-(first&15);
  /* Trim unchanged tail; preserve register holes and the3731/3236 limits. */
  if(!commit)while(count>1 && !uncertain[chip][first+count-1] && desired[chip][first+count-1]==sent[chip][first+count-1])count--;
  uint8_t zero=0,reg=commit?0x25:(chip>=2 && chip<5?0x24:1)+first;
  const uint8_t *data=commit?&zero:desired[chip]+first;
  stats.transactions++;stats.bytes+=count+2;next_chip=(chip+1)%6;
  if(!write(chip,reg,data,count)){stats.failures++;retrying[chip]=true;retry_at[chip]=now+100;if(!commit){memset(uncertain[chip]+first,1,count);if(chip==5)latch=true;}return false;}
  retrying[chip]=false;
  if(commit)latch=false;
  else{memcpy(sent[chip]+first,data,count);memset(uncertain[chip]+first,0,count);cursor[chip]=(first+count)%limit;if(chip==5)latch=true;}
  if(was_pending && !my2k_refresh_pending()){stats.completed++;was_pending=false;}
  return true;
 }
 return false;
}
