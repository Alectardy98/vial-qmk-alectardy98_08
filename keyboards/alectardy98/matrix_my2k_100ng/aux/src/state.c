/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_state.h"
bool aux_state_apply(aux_state *s,const my2k_wire_message *m,bool *reply){
    if(!s||!m||!reply)return false;
    *reply=false;
    switch(m->type){
    case MY2K_LED:s->leds=m->v[0];*reply=true;return true;
    case MY2K_LYR:s->layer=m->v[0];*reply=true;return true;
    case MY2K_TH:s->temperature=m->v[0];s->humidity=m->v[1];return true;
    case MY2K_TM:s->year=m->year;for(unsigned n=0;n<6;n++)s->datetime[n]=m->v[n];return true;
    case MY2K_KH:s->key_events++;return true;
    case MY2K_SCR:*reply=true;return false; /* Unknown physical panel/power contract. */
    case MY2K_STS:return true;
    case MY2K_TC:return false; /* AUX produces touch; never applies incoming sensor values. */
    default:return false;
    }
}
