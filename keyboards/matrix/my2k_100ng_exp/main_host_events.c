/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "hal.h"
#include "usbh/internal.h"
#include "timer.h"
#include "main_host_events.h"
#include <string.h>
#if MY2K_DEV13_PROFILE
#include "main_host_debounce.h"
#endif
#define EVENT_CAPACITY 64u
/* Payload bytes: schema,index,kind,packedstates, time,ms,low,upper,HPRT,callerPC. */
static uint8_t events[EVENT_CAPACITY][28];
static uint32_t count,dropped,debounces,reset_waits,reset_polls,last_time;
static uint32_t last_low,last_upper,last_ms;
static uint8_t last_state;
static bool have_last;
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
enum {PASS_ENTER=1,PASS_RETURN,SLEEP_ENTER,SLEEP_RETURN};
static void capture(uint8_t kind,uint32_t ms,uint32_t caller,bool force){
    osalSysLock();
    usbh_port_t *port=&USBHD1.rootport;
    uint32_t low=(uint32_t)port->lld_status|((uint32_t)port->lld_c_status<<16);
    uint32_t upper=(uint32_t)port->status|((uint32_t)port->c_status<<16);
    uint8_t state=(uint8_t)port->device.status|((uint8_t)USBHD1.status<<4);
    /* Ignore identical idle passes and reset polls. The periodic Dev10 sampler
     * already observes physical-only changes; this log preserves first causes. */
    if(!force && have_last && low==last_low && upper==last_upper && ms==last_ms && state==last_state){osalSysUnlock();return;}
    have_last=true;last_low=low;last_upper=upper;last_ms=ms;last_state=state;
    uint32_t now=timer_read32();last_time=now;
    if(count>=EVENT_CAPACITY){dropped++;osalSysUnlock();return;}
    uint8_t *p=events[count];memset(p,0,28);p[0]=1;p[1]=(uint8_t)count;p[2]=kind;p[3]=state;
    put32(p+4,now);put32(p+8,ms);put32(p+12,low);put32(p+16,upper);
    if(USBHD1.otg && (RCC->AHB2ENR&RCC_AHB2ENR_OTGFSEN))put32(p+20,USBHD1.otg->HPRT);
    put32(p+24,caller);count++;
    osalSysUnlock();
}
__attribute__((noinline,noclone,externally_visible))
void my2k_host_events_pass(bool entering){
    capture(entering?PASS_ENTER:PASS_RETURN,0,(uint32_t)(uintptr_t)__builtin_return_address(0),false);
}
__attribute__((noinline,noclone,externally_visible))
void my2k_host_events_sleep_ms(uint32_t ms){
    uint32_t caller=(uint32_t)(uintptr_t)__builtin_return_address(0);
    for (unsigned attempt = 0; attempt < 2; attempt++) {
        osalSysLock();
        if(ms==200u && USBHD1.rootport.device.status==USBH_DEVSTATUS_ATTACHED)debounces++;
        if(ms==20u && USBHD1.rootport.device.status==USBH_DEVSTATUS_CONNECTED)reset_waits++;
        if(ms==1u && USBHD1.rootport.device.status==USBH_DEVSTATUS_CONNECTED)reset_polls++;
        osalSysUnlock();
        capture(SLEEP_ENTER,ms,caller,ms!=1u);
        /* Recorder TU retains the original OSAL macro, never its hook. */
        osalThreadSleepMilliseconds(ms);
        capture(SLEEP_RETURN,ms,caller,ms!=1u);
#if MY2K_DEV13_PROFILE
        /* One event-driven fresh window, not a larger fixed delay or a retry
         * loop. Preserve the raw return above before acknowledging the edge.
         * Further changes go untouched to the original upper-layer abort. */
        if (attempt == 0 && ms == HAL_USBH_PORT_DEBOUNCE_TIME &&
            my2k_host_debounce_rearm()) {
            capture(5,ms,caller,true); /* DEBOUNCE_RESTART */
            continue;
        }
#endif
        break;
    }
}
__attribute__((noinline,noclone,externally_visible))
void my2k_host_events_summary(uint8_t report[32]){
    osalSysLock();memset(report+4,0,28);report[4]=1;report[5]=(uint8_t)count;report[6]=EVENT_CAPACITY;report[7]=dropped!=0;
    put32(report+8,count);put32(report+12,dropped);put32(report+16,debounces);
    put32(report+20,reset_waits);put32(report+24,reset_polls);put32(report+28,last_time);osalSysUnlock();
}
__attribute__((noinline,noclone,externally_visible))
bool my2k_host_events_page(uint8_t index,uint8_t report[32]){
    osalSysLock();if(index>=count){osalSysUnlock();return false;}
    memcpy(report+4,events[index],28);osalSysUnlock();return true;
}
