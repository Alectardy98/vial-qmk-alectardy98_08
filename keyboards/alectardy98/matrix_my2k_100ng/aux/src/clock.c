/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_clock.h"
static unsigned days(unsigned year,unsigned month){
 static const uint8_t lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
 return month==2&&year%4==0&&(year%100!=0||year%400==0)?29:lengths[month-1];
}
bool aux_datetime_set(aux_datetime *clock,const my2k_wire_message *m,uint32_t now){
 if(!clock||!m||m->type!=MY2K_TM||m->year>9999)return false;
 char validation[MY2K_WIRE_MAX];if(my2k_wire_encode(m,validation,sizeof validation)<0)return false;
 *clock=(aux_datetime){.year=m->year,.month=m->v[0],.day=m->v[1],.weekday=m->v[2],.hour=m->v[3],.minute=m->v[4],.second=m->v[5],.anchor_ms=now,.valid=true};return true;
}
bool aux_datetime_step(aux_datetime *clock,uint32_t now){
 if(!clock||!clock->valid)return false;
 uint32_t elapsed=(uint32_t)(now-clock->anchor_ms)/1000u;if(!elapsed)return false;
 clock->anchor_ms+=elapsed*1000u;
 uint32_t seconds=(uint32_t)clock->hour*3600u+clock->minute*60u+clock->second+elapsed;
 clock->hour=(uint8_t)(seconds/3600u%24u);clock->minute=(uint8_t)(seconds/60u%60u);clock->second=(uint8_t)(seconds%60u);
 for(uint32_t d=seconds/86400u;d;d--){clock->weekday=clock->weekday==7?1:clock->weekday+1;if(++clock->day>days(clock->year,clock->month)){clock->day=1;if(++clock->month>12){clock->month=1;if(++clock->year>9999){clock->valid=false;return true;}}}}
 return true;
}
