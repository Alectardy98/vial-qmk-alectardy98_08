/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_ring.h"
#include <stdatomic.h>
#define MASK (AUX_RING_CAPACITY-1u)
_Static_assert((AUX_RING_CAPACITY&MASK)==0,"ring capacity must be a power of two");
_Static_assert(AUX_RING_CAPACITY<=UINT16_MAX,"ring index must fit");
void aux_ring_init(aux_ring *q){if(q){q->head=0;q->tail=0;}}
size_t aux_ring_count(const aux_ring *q){return q?(q->head-q->tail)&MASK:0;}
bool aux_ring_push(aux_ring *q,const uint8_t *bytes,size_t size){
 if(!q||(!bytes&&size)||size>MASK)return false;
 uint16_t head=q->head,tail=q->tail;
 atomic_signal_fence(memory_order_acquire);
 if(size>MASK-((head-tail)&MASK))return false;
 for(size_t i=0;i<size;i++)q->bytes[(head+i)&MASK]=bytes[i];
 atomic_signal_fence(memory_order_release);
 q->head=(uint16_t)((head+size)&MASK);return true;
}
bool aux_ring_pop(aux_ring *q,uint8_t *byte){
 if(!q||!byte)return false;
 uint16_t tail=q->tail,head=q->head;
 atomic_signal_fence(memory_order_acquire);
 if(tail==head)return false;
 *byte=q->bytes[tail];
 atomic_signal_fence(memory_order_release);
 q->tail=(uint16_t)((tail+1u)&MASK);return true;
}
