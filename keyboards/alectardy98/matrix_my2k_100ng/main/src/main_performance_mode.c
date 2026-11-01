/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_performance_mode.h"
#include <string.h>
#if !MY2K_PERFORMANCE_HOST
#include QMK_KEYBOARD_H
#include "eeconfig.h"
#include "vial.h"
#include "wait.h"
#include "usb_main.h"
#include "main_eeprom.h"
#include "main_persist.h"
#include "main_storage_mode.h"
#include "my2k_usbh_chibios.h"
#include "my2k_usb_bus_hooks.h"
#if MY2K_GAMES_PROFILE
#include "main_games.h"
#endif
static uint32_t my2k_performance_platform_read(void){return eeconfig_read_user();}
static void my2k_performance_platform_write(uint32_t v){eeconfig_update_user(v);}
static bool my2k_performance_platform_durable(void){return my2k_eeprom_durable();}
static bool my2k_performance_platform_pending(void){return my2k_eeprom_pending();}
static bool my2k_performance_platform_busy(void){return my2k_persist_busy();}
static bool my2k_performance_platform_idle(void){return !my2k_asset_transition_pending()&&my2k_usbh_asset_idle()
#if MY2K_GAMES_PROFILE
    &&!my2k_games_running()
#endif
    ;}
static bool my2k_performance_platform_export(void){return my2k_asset_export_active();}
static bool my2k_performance_platform_unlocked(void){return vial_unlocked;}
static uint32_t my2k_performance_platform_error(void){return my2k_persist_error();}
static uint32_t my2k_performance_platform_now(void){return timer_read32();}
static void my2k_performance_platform_usb_fallback(void){restart_usb_driver(&USB_DRIVER);}
static void my2k_performance_platform_restart(void){clear_keyboard();usbDisconnectBus(&USB_DRIVER);wait_ms(11);NVIC_SystemReset();}
#endif
static bool initialized,requested,boot_high,hs_enabled,export_override,pending;
static uint32_t boot_started;
static volatile bool configured,enumerated;
static volatile uint8_t speed;
static volatile uint32_t faults,enum_count;
static uint32_t requested_at,generation,last_error;
__attribute__((noipa,externally_visible)) void my2k_performance_init(void){
    if(initialized)return;
    initialized=true;boot_started=my2k_performance_platform_now();
    uint32_t word=my2k_performance_platform_read();
    requested=(word&~MY2K_PERFORMANCE_BIT)==MY2K_BRIGHTNESS_MARKER&&(word&MY2K_PERFORMANCE_BIT);
    export_override=my2k_performance_platform_export();boot_high=requested&&!export_override;hs_enabled=boot_high;
}
__attribute__((noipa,externally_visible)) bool my2k_performance_mode_changing(void){return pending;}
bool my2k_performance_hs_requested(void){return hs_enabled;}
bool my2k_performance_high_cpu(void){return boot_high;}
bool my2k_performance_usb_start_failed(void){return (faults&8u)!=0;}
void my2k_performance_usb_fault(uint32_t bits){faults|=bits;if(bits&4u)hs_enabled=false;}
void my2k_performance_usb_enumerated(uint8_t raw){
    enum_count++;enumerated=true;
    if(raw==0&&hs_enabled)speed=2;
    else if(raw==1||raw==3)speed=1;
    else{speed=0;faults|=0x10u;}
}
void my2k_performance_usb_configured(bool on){configured=on;}
void my2k_performance_usb_reset(void){configured=false;enumerated=false;speed=0;boot_started=my2k_performance_platform_now();}
uint8_t my2k_performance_usb_speed(void){return enumerated?speed:0;}
static bool set_mode(uint8_t mode,uint32_t now){
    if(mode>1||!my2k_performance_platform_unlocked()||my2k_performance_platform_export()||!my2k_performance_platform_idle())return false;
    if(pending)return mode==(uint8_t)requested;
    if(!my2k_performance_platform_durable()||my2k_performance_platform_pending()||my2k_performance_platform_busy()){last_error=1;return false;}
    if(mode==(uint8_t)requested&&mode==(uint8_t)boot_high&&!faults)return true;
    uint32_t old=my2k_performance_platform_read();
    if((old&~MY2K_PERFORMANCE_BIT)!=MY2K_BRIGHTNESS_MARKER){last_error=2;return false;}
    uint32_t next=(old&~MY2K_PERFORMANCE_BIT)|(mode?MY2K_PERFORMANCE_BIT:0);
    if(old!=next)my2k_performance_platform_write(next);
    if(my2k_performance_platform_read()!=next){last_error=3;return false;}
    requested=mode;generation++;pending=true;requested_at=now;last_error=0;return true;
}
void my2k_performance_task(uint32_t now){
    if(!pending){
        /* One bounded controller-only recovery if a successful HS core start
         * never produces enumeration. Never erase preference or reboot-loop. */
        if(hs_enabled&&!enumerated&&!my2k_performance_usb_start_failed()&&(uint32_t)(now-boot_started)>=3000u){
            my2k_performance_usb_fault(0x24u);my2k_performance_usb_reset();
            my2k_performance_platform_usb_fallback();
        }
        return;
    }
    uint32_t error=my2k_performance_platform_error();
    if(!my2k_performance_platform_durable()){last_error=5;pending=false;return;}
    if(error||(uint32_t)(now-requested_at)>10000u){last_error=error?error:4;pending=false;return;}
    if((uint32_t)(now-requested_at)<500u||my2k_performance_platform_pending()||my2k_performance_platform_busy()||!my2k_performance_platform_idle())return;
    pending=false;my2k_performance_platform_restart();
}
static void put16(uint8_t*p,uint16_t v){p[0]=v;p[1]=v>>8;}
static void put32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
__attribute__((noipa,externally_visible)) bool my2k_performance_reply(uint8_t p[32]){
    if(!p||p[0]!=0xfd||(p[1]!=75&&p[1]!=76))return false;
    uint8_t command=p[1],mode=p[2];bool ok=command==75||set_mode(mode,my2k_performance_platform_now());
    memset(p,0,32);p[0]=0xfd;p[1]=command;p[2]=ok?0xaa:0x55;p[3]=1;p[4]=requested;
    uint8_t live=configured&&enumerated?speed:0;
    p[5]=live?(live==2?1:0):255;
    p[6]=(pending?1:0)|2|(my2k_performance_platform_pending()?4:0)|(enumerated&&speed==2?8:0)|(faults?16:0)|
        (requested&&live==1&&!export_override?32:0)|(export_override?64:0);
    p[7]=live;put16(p+8,live?(live==2?125:1000):0);put16(p+10,boot_high?125:1000);
    put32(p+12,faults);put32(p+16,generation);put32(p+20,last_error);put32(p+24,enum_count);return true;
}
