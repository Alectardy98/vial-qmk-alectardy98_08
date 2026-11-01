/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "hal.h"
#include "usbh/internal.h"
#include "main_host_mode.h"
#include <string.h>

static uint32_t pre_result,post_result,pre_gccfg,gusbcfg,gccfg,pcgcctl,gintsts;
static uint8_t stages;
/* Called from the host LLD while host sources remain masked; the existing
 * wrapper unlocks the scheduler/PC IRQs around each bounded wait. Original
 *08036310 ignores08036C00 failure and continues08036D6C. Dev8 completes that
 * configuration after a prewait timeout, but requires a successful postwait
 * before publishing STARTED/enabling host interrupts. No automatic retry. */
uint32_t my2k_dev8_host_mode(USBHDriver *host,bool final){
    uint32_t result=_usbh_wait_host_modeS(host);
    if(final){post_result=result;stages|=2;}
    else {pre_result=result;pre_gccfg=host->otg->GCCFG;stages|=1;}
    gusbcfg=host->otg->GUSBCFG;gccfg=host->otg->GCCFG;
    pcgcctl=host->otg->PCGCCTL;gintsts=host->otg->GINTSTS;
    return result;
}
static void put32(uint8_t *p,uint32_t value){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(value>>(i*8));}
void my2k_host_mode_status(uint8_t report[32]){
    memset(report+4,0,28);report[4]=1;report[5]=stages;
    report[6]=(uint8_t)pre_result;report[7]=(uint8_t)(pre_result>>8);
    report[8]=(uint8_t)post_result;report[9]=(uint8_t)(post_result>>8);
    put32(report+12,gusbcfg);put32(report+16,gccfg);put32(report+20,pcgcctl);
    put32(report+24,gintsts);put32(report+28,pre_gccfg);
}

#if MY2K_DEV9_PROFILE
static uint32_t port_hprt,port_gintsts,port_gintmsk,port_gahbcfg;
static uint8_t port_flags;
/* Dev8's shared LLD powers HPRT with a read/modify/write that can acknowledge
 * an already pending connection edge, then initializes its software cache to
 * POWER only. Stock08036EB8 masks those change flags. Without editing shared
 * code, reconcile one actual connected level after startup, before the host
 * worker consumes its root-hub cache. A still-pending hardware connection
 * edge is left to its normal ISR. This never resets or writes the port. */
__attribute__((noinline,noclone,externally_visible)) void my2k_dev9_host_discovery(void){
    osalSysLock();
    if(USBHD1.status==USBH_STATUS_STARTED){
        port_hprt=USBHD1.otg->HPRT;port_gintsts=USBHD1.otg->GINTSTS;
        port_gintmsk=USBHD1.otg->GINTMSK;port_gahbcfg=USBHD1.otg->GAHBCFG;
        port_flags=1;
        if(port_hprt&HPRT_PCSTS)port_flags|=2;
        if(port_hprt&HPRT_PPWR)port_flags|=4;
        if(port_hprt&HPRT_POCA)port_flags|=8;
        if((port_hprt&(HPRT_PCSTS|HPRT_PPWR|HPRT_POCA|HPRT_PCDET))==(HPRT_PCSTS|HPRT_PPWR) &&
           !(USBHD1.rootport.lld_status&USBH_PORTSTATUS_CONNECTION)){
            USBHD1.rootport.lld_status|=USBH_PORTSTATUS_CONNECTION;
            USBHD1.rootport.lld_c_status|=USBH_PORTSTATUS_C_CONNECTION;
            port_flags|=16;
        }
    }
    osalSysUnlock();
}
void my2k_host_port_status(uint8_t report[32]){
    memset(report+4,0,28);report[4]=1;report[5]=port_flags;
    report[6]=(uint8_t)USBHD1.rootport.device.status;report[7]=(uint8_t)USBHD1.status;
    put32(report+8,port_hprt);put32(report+12,port_gintsts);put32(report+16,port_gintmsk);
    put32(report+20,port_gahbcfg);put32(report+24,USBHD1.rootport.lld_status);
    put32(report+28,USBHD1.rootport.lld_c_status);
}
#endif
