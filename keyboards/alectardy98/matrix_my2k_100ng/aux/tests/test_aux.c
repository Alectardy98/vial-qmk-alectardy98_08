/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "aux_platform.h"
#include "aux_state.h"
static void field(uint32_t w[4],unsigned hi,unsigned lo,uint32_t value){for(unsigned b=lo;b<=hi;b++){unsigned i=(127-b)/32,shift=b%32;w[i]=(w[i]&~(1u<<shift))|(((value>>(b-lo))&1u)<<shift);}}
int main(void){
 uint32_t w[4]={0},n=0;field(w,127,126,1);field(w,69,48,121);assert(aux_csd_sectors(w,&n)&&n==122*1024u);
 memset(w,0,sizeof w);field(w,83,80,9);field(w,73,62,1935);field(w,49,47,5);assert(aux_csd_sectors(w,&n)&&n==247808);
 field(w,127,126,3);assert(!aux_csd_sectors(w,&n));assert(!aux_csd_sectors(NULL,&n));assert(!aux_csd_sectors(w,NULL));
 aux_state state={0};my2k_wire_message m={.type=MY2K_LED,.v={3}};bool reply=false;
 assert(aux_state_apply(&state,&m,&reply)&&reply&&state.leds==3);
 m.type=MY2K_LYR;m.v[0]=7;assert(aux_state_apply(&state,&m,&reply)&&reply&&state.layer==7);
 m.type=MY2K_SCR;assert(!aux_state_apply(&state,&m,&reply)&&reply&&!state.panel_ready);
 m.type=MY2K_TC;assert(!aux_state_apply(&state,&m,&reply)&&!reply);
 m.type=MY2K_TH;m.v[0]=25;m.v[1]=56;assert(aux_state_apply(&state,&m,&reply)&&!reply&&state.temperature==25&&state.humidity==56);
 puts("AUX CSD and RAM-only dispatch: PASS");return 0;
}
