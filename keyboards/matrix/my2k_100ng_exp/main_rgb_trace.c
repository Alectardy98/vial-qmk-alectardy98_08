/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_rgb_trace.h"
#include <string.h>
#define CAPACITY 64u
typedef struct {uint32_t seq,ms,caller;uint8_t addr,reg,count,flags;int8_t status;uint8_t fault,phase,data[8];} entry;
static entry ring[CAPACITY],view[CAPACITY];
static uint8_t view_used;
static uint32_t sequence,writes,reads,nonbank,frozen_at,overwritten;
static uint8_t used,next;
__attribute__((noipa,externally_visible))
void my2k_rgb_trace_record(bool read,uint8_t addr,uint8_t reg,const uint8_t *data,uint16_t count,int status,uint8_t fault,uint8_t phase,uintptr_t caller,uint32_t now){
 sequence++;if(read)reads++;else{writes++;if(reg!=0xfd||count!=1)nonbank++;}
 if(frozen_at)return;
 entry *e=&ring[next];memset(e,0,sizeof *e);e->seq=sequence;e->ms=now;e->caller=(uint32_t)caller;e->addr=addr;e->reg=reg;
 e->count=count>255?255:count;e->status=status;e->fault=fault;e->phase=phase;e->flags=(read?1:2)|(count>8?4:0)|(status?8:0);
 if(data&&count<=64&&((read&&!status)||(!read&&(!status||phase!=1)))){unsigned n=count>8?8:count;memcpy(e->data,data,n);}
 if(used<CAPACITY)used++;else overwritten++;
 next=(next+1)%CAPACITY;if(status)frozen_at=sequence;
}
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
__attribute__((noipa,externally_visible))
bool my2k_rgb_trace_page(uint8_t page,uint8_t index,uint8_t p[32]){
 if(!p||(page!=41&&page!=42)||(page==41?index!=0:index>=view_used))return false;
 memset(p+4,0,28);p[4]=1;
 if(page==41){
  /* GET41 latches a RAM-only view so a64report HID download cannot race a
   * rolling trace. It does not arm sampling or touch the peripheral. */
  for(unsigned i=0;i<used;i++)view[i]=ring[(next+CAPACITY-used+i)%CAPACITY];
  view_used=used;
  p[5]=used;p[6]=CAPACITY;p[7]=frozen_at!=0;put(p+8,sequence);put(p+12,writes);put(p+16,reads);put(p+20,nonbank);put(p+24,frozen_at);put(p+28,overwritten);return true;}
 const entry *e=&view[index];p[5]=e->addr;p[6]=e->count;p[7]=e->flags;
 put(p+8,e->seq);put(p+12,e->ms);put(p+16,e->caller);p[20]=e->reg;p[21]=(uint8_t)e->status;p[22]=e->fault;p[23]=e->phase;memcpy(p+24,e->data,8);return true;
}
