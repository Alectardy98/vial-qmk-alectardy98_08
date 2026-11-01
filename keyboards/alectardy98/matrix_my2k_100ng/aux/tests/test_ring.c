/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "aux_ring.h"
#include "my2k_wire.h"
static void drain(aux_ring *q,const uint8_t *expected,size_t count){uint8_t byte;for(size_t i=0;i<count;i++){assert(aux_ring_pop(q,&byte));assert(byte==expected[i]);}assert(!aux_ring_pop(q,&byte));}
int main(void){aux_ring q;aux_ring_init(&q);uint8_t all[511],byte;
 for(size_t i=0;i<sizeof all;i++)all[i]=(uint8_t)i;
 assert(!aux_ring_pop(&q,&byte)&&aux_ring_count(&q)==0);
 assert(aux_ring_push(&q,all,sizeof all)&&aux_ring_count(&q)==511);
 assert(!aux_ring_push(&q,all,1)&&aux_ring_count(&q)==511);drain(&q,all,sizeof all);
 /* The indices now sit at511; a frame crosses the ring boundary intact. */
 assert(aux_ring_push(&q,all,300));for(unsigned i=0;i<200;i++){assert(aux_ring_pop(&q,&byte)&&byte==all[i]);}
 assert(!aux_ring_push(&q,all,412)&&aux_ring_count(&q)==100);
 assert(aux_ring_push(&q,all,411)&&aux_ring_count(&q)==511);
 for(unsigned i=200;i<300;i++){assert(aux_ring_pop(&q,&byte)&&byte==all[i]);}
 drain(&q,all,411);
 assert(!aux_ring_push(NULL,all,1)&&!aux_ring_push(&q,NULL,1)&&!aux_ring_push(&q,all,512));
 assert(aux_ring_push(&q,NULL,0)&&!aux_ring_pop(&q,NULL));
 /* Exercise actual production framing through actual production queue and parser. */
 my2k_wire_message message={.type=MY2K_TC,.v={0,255,1,254,2,253,3,252}},parsed;
 char encoded[128];int size=my2k_wire_encode(&message,encoded,sizeof encoded);assert(size>32);
 assert(aux_ring_push(&q,(const uint8_t *)encoded,(size_t)size));
 assert(aux_ring_push(&q,(const uint8_t *)encoded,(size_t)size));
 my2k_wire_parser parser;my2k_wire_init(&parser);unsigned frames=0,passes=0;
 while(aux_ring_count(&q)){
  unsigned processed=0;
  for(;processed<32&&aux_ring_pop(&q,&byte);processed++){
   int result=my2k_wire_feed(&parser,byte,&parsed);assert(result>=0);
   if(result){frames++;assert(parsed.type==MY2K_TC&&!memcmp(parsed.v,message.v,8));}
  }
  assert(processed<=32);passes++;
 }
 assert(frames==2&&passes>2);
 /* Reject a whole valid frame near capacity; already accepted data is intact. */
 assert(aux_ring_push(&q,all,511-(size_t)size+1));
 size_t before=aux_ring_count(&q);assert(!aux_ring_push(&q,(const uint8_t *)encoded,(size_t)size)&&aux_ring_count(&q)==before);
 drain(&q,all,before);
 puts("AUX production SPSC ring: full/empty/wrap/frame-atomic rejection and parser-budget stream PASS");return 0;}
