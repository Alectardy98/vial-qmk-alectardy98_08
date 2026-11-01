/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_aux_telemetry.h"
#include "aux_link.h"
#if MY2K_DEV21_PROFILE
#include "my2k_usbh_chibios.h"
#include "my2k_storage_caps.h"
#endif
#include <string.h>
static uint8_t cached[MY2K_DIAG_PAGES][8],next_page;
static uint32_t received_at[MY2K_DIAG_PAGES],last_attempt;
static bool valid[MY2K_DIAG_PAGES],schema;
#if MY2K_DEV7_PROFILE
static uint32_t service_calls,queries_sent,replies_received,schema_rejected,unresponsive_calls;
#endif
static void put32(uint8_t *p,uint32_t value){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(value>>(i*8));}
__attribute__((noinline,noclone,externally_visible)) void my2k_aux_telemetry_service(uint32_t now){
#if MY2K_DEV7_PROFILE
    service_calls++;
#endif
    if(!my2k_aux_responsive()){
#if MY2K_DEV7_PROFILE
        unresponsive_calls++;
#endif
        memset(valid,0,sizeof valid);schema=false;next_page=0;return;
    }
#if MY2K_DEV21_PROFILE
    if((uint32_t)(now-last_attempt)<200u)return;
    my2k_usbh_identity identity;my2k_usbh_get_identity(&identity);
    my2k_storage_caps peer=my2k_storage_negotiate(identity.vendor_id,identity.product_id,identity.device_valid,identity.inquiry_valid,identity.inquiry);
    bool display21=peer.version==21
#if MY2K_RELEASE1_PROFILE
        ||peer.version==24
#endif
        ;
    unsigned pages=display21?MY2K_DIAG_PAGES:6;
    if(!display21&&(uint32_t)(now-last_attempt)<2000u)return;
    if(next_page>=pages)next_page=0;
#else
    if((uint32_t)(now-last_attempt)<2000u)return;
#endif
    last_attempt=now;
    my2k_wire_message request={.type=MY2K_DQ,.v={next_page}};
    if(my2k_aux_send(&request)){
#if MY2K_DEV7_PROFILE
        queries_sent++;
#endif
        next_page=(next_page+1u)%
#if MY2K_DEV21_PROFILE
        pages;
#else
        MY2K_DIAG_PAGES;
#endif
    }
}
__attribute__((noinline,noclone,externally_visible)) void my2k_aux_telemetry_receive(const my2k_wire_message *message,uint32_t now){
    if(!message||message->type!=MY2K_DR||message->v[0]>=MY2K_DIAG_PAGES)return;
#if MY2K_DEV7_PROFILE
    replies_received++;
    if(message->v[0]==0 && message->v[1]!=6)schema_rejected++;
#endif
    unsigned page=message->v[0];
    if(!page){schema=message->v[1]==6;if(!schema){memset(valid,0,sizeof valid);return;}}
    if(!schema)return;
    memcpy(cached[page],message->v,8);received_at[page]=now;valid[page]=true;
}
__attribute__((noinline,noclone,externally_visible)) void my2k_aux_telemetry_page(uint8_t page,uint8_t report[32],uint32_t now){
    if(page>=MY2K_DIAG_PAGES){report[3]=255;return;}
    report[4]=page;report[5]=valid[page]&&schema;
    if(report[5]){
        memcpy(report+8,cached[page],8);put32(report+16,now-received_at[page]);
    }
    report[6]=6; /* Expected diagnostic schema, not an unobserved AUX identity. */
}

#if MY2K_DEV7_PROFILE
void my2k_aux_telemetry_counters(uint8_t report[32]){
    report[4]=6;report[6]=next_page;
    for(unsigned i=0;i<MY2K_DIAG_PAGES;i++)if(valid[i]){
        if(i<8)report[5]|=1u<<i;
#if MY2K_DEV21_PROFILE
        else if(i<16)report[7]|=1u<<(i-8);
#endif
    }
    put32(report+8,service_calls);put32(report+12,queries_sent);put32(report+16,replies_received);
    put32(report+20,schema_rejected);put32(report+24,unresponsive_calls);put32(report+28,last_attempt);
}
#endif
