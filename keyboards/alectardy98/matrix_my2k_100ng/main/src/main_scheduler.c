/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "hal.h"
#include "timer.h"
#include "main_scheduler.h"
#include <string.h>
static thread_t *host_worker,*media_worker;
static uint32_t yield_count;
static uint8_t snapshot[28];
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
void my2k_scheduler_workers(thread_t *host,thread_t *media){
    chSysLock();host_worker=host;media_worker=media;chSysUnlock();
}
__attribute__((noinline,noclone,externally_visible))
void my2k_main_cooperate(void){
    uint32_t now=timer_read32();
    chSysLock();
    snapshot[0]=1;snapshot[1]=4u|(host_worker?1u:0u)|(media_worker?2u:0u);
    snapshot[2]=(uint8_t)CH_CFG_TIME_QUANTUM;
    put32(snapshot+4,chVTGetSystemTimeX());put32(snapshot+8,++yield_count);put32(snapshot+12,now);
    snapshot[20]=host_worker?(uint8_t)host_worker->state:255;
    snapshot[21]=host_worker?(uint8_t)host_worker->hdr.pqueue.prio:255;
    snapshot[22]=media_worker?(uint8_t)media_worker->state:255;
    snapshot[23]=media_worker?(uint8_t)media_worker->hdr.pqueue.prio:255;
    snapshot[24]=(uint8_t)chThdGetSelfX()->state;
    snapshot[25]=(uint8_t)chThdGetSelfX()->hdr.pqueue.prio;
    snapshot[26]=(uint8_t)currcore->rlist.pqueue.next->prio;
    chSysUnlock();
    /* Main/host/media all use NORMALPRIO and CH_CFG_TIME_QUANTUM is zero.
     * Main's steady scan loop otherwise never grants ready equal-priority
     * workers CPU time after startup sleeps finish. Yield without a delay,
     * priority boost, shared scheduler change or an active peripheral action. */
    chThdYield();
    now=timer_read32();chSysLock();put32(snapshot+16,now);chSysUnlock();
}
__attribute__((noinline,noclone,externally_visible))
void my2k_scheduler_status(uint8_t report[32]){
    chSysLock();memcpy(report+4,snapshot,28);chSysUnlock();
}
