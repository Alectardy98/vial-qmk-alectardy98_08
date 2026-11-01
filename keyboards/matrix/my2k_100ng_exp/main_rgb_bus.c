/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Bounded, write-only I2C on manufacturer PB6/PB7; no STM32 DMA dependency.
 * Both pins are open-drain: high means release, never a driven high level. */
#include "main_rgb_bus.h"
#if MY2K_DEV18_PROFILE
#include "main_rgb_trace.h"
static uint8_t read_phase,read_first_fault;
#define READ_PHASE(n) (read_phase=(n))
#else
#define READ_PHASE(n) ((void)0)
#endif
#if MY2K_DEV4_PROFILE
#include "main_perf.h"
#endif
#ifdef MY2K_RGB_BUS_HOST
extern void my2k_bus_configure(void);
extern void my2k_bus_scl(bool);
extern void my2k_bus_sda(bool);
extern bool my2k_bus_read_scl(void);
extern bool my2k_bus_read_sda(void);
extern void wait_us(unsigned);
extern uint32_t timer_read32(void);
#else
#include "hal.h"
#include "wait.h"
#include "timer.h"
static void my2k_bus_configure(void) {
    /* Match stock/full-width BSRR writes, as in the working matrix driver. */
    palWritePad(GPIOB,6,true);palWritePad(GPIOB,7,true);
    palSetLineMode(PAL_LINE(GPIOB,6),PAL_MODE_OUTPUT_OPENDRAIN);
    palSetLineMode(PAL_LINE(GPIOB,7),PAL_MODE_OUTPUT_OPENDRAIN);
}
static void my2k_bus_scl(bool release) { palWritePad(GPIOB,6,release); }
static void my2k_bus_sda(bool release) { palWritePad(GPIOB,7,release); }
static bool my2k_bus_read_scl(void) { return palReadLine(PAL_LINE(GPIOB,6)); }
static bool my2k_bus_read_sda(void) { return palReadLine(PAL_LINE(GPIOB,7)); }
#endif
static uint8_t fault;
static uint32_t recoveries,started;
static uint16_t deadline;
#if MY2K_DEV10_PROFILE
#include <string.h>
static my2k_rgb_bus_failure_diagnostics failure_cache[6];
static uint8_t write_phase, write_first_fault;
static uint16_t write_data_index;
#define WRITE_PHASE(phase,index) do { write_phase=(phase);write_data_index=(index); } while(0)
bool my2k_rgb_bus_failure(unsigned controller, my2k_rgb_bus_failure_diagnostics *out) {
    if(!out)return false;
    if(controller>=6){memset(out,0,sizeof *out);return false;}
    *out=failure_cache[controller];return true;
}
static int failure_index(uint8_t address) {
    const uint8_t addresses[6]={0x6e,0x68,0xe8,0xee,0xea,0x78};
    for(unsigned i=0;i<6;i++)if(addresses[i]==address)return (int)i;
    return -1;
}
#else
#define WRITE_PHASE(phase,index) ((void)0)
#endif
static bool raise_clock(void) {
    my2k_bus_scl(true);
    for (unsigned polls=0;polls<10000;polls++) {
        if (my2k_bus_read_scl()) return true;
        if ((uint32_t)(timer_read32()-started)>=deadline) break;
        wait_us(1);
    }
    fault=1;return false;
}
static void stop(void) {
    my2k_bus_scl(false);my2k_bus_sda(false);wait_us(2);
    (void)raise_clock();wait_us(2);my2k_bus_sda(true);wait_us(2);
}
void my2k_rgb_bus_init(void) {
    fault=0;started=timer_read32();deadline=5;
    my2k_bus_configure();my2k_bus_sda(true);
    if (!raise_clock()) return;
    if (!my2k_bus_read_sda()) {
        recoveries++;
        for (unsigned i=0;i<9;i++) { my2k_bus_scl(false);wait_us(2);if(!raise_clock())break;wait_us(2); }
        stop();
        if (!my2k_bus_read_sda()) fault=2;
    }
}
static bool byte(uint8_t value) {
    for (unsigned bit=0;bit<8;bit++) {
        my2k_bus_scl(false);my2k_bus_sda((value&0x80)!=0);wait_us(2);
        if (!raise_clock()) return false;
        wait_us(2);my2k_bus_scl(false);value<<=1;
    }
    my2k_bus_sda(true);wait_us(2);
    if (!raise_clock()) return false;
    wait_us(2);bool ack=!my2k_bus_read_sda();my2k_bus_scl(false);
    if (!ack) fault=3;
    return ack;
}
static i2c_status_t write_transaction(uint8_t address,uint8_t reg,const uint8_t *data,uint16_t count,uint16_t timeout) {
    /* Whitelist exactly the recovered LED controllers; never touch AUX/storage. */
    WRITE_PHASE(MY2K_RGB_PHASE_VALIDATE,UINT16_MAX);
    if ((address!=0x6e && address!=0x68 && address!=0xe8 && address!=0xee && address!=0xea && address!=0x78) || !data || !count || count>64 || !timeout) { fault=4;return I2C_STATUS_ERROR; }
    fault=0;started=timer_read32();deadline=timeout;
    WRITE_PHASE(MY2K_RGB_PHASE_START,UINT16_MAX);
    my2k_bus_sda(true);
    if (!raise_clock()) { my2k_bus_sda(true);return I2C_STATUS_TIMEOUT; }
    if (!my2k_bus_read_sda()) { fault=2;return I2C_STATUS_ERROR; }
    wait_us(2);my2k_bus_sda(false);wait_us(2);my2k_bus_scl(false);
    WRITE_PHASE(MY2K_RGB_PHASE_ADDRESS,UINT16_MAX);
    bool ok=byte(address);
    if(ok){WRITE_PHASE(MY2K_RGB_PHASE_REGISTER,UINT16_MAX);ok=byte(reg);}
    for (unsigned i=0;ok && i<count;i++) {
        WRITE_PHASE(MY2K_RGB_PHASE_DATA,i);
        if ((uint32_t)(timer_read32()-started)>=deadline) { fault=1;ok=false;break; }
        ok=byte(data[i]);
    }
#if MY2K_DEV10_PROFILE
    uint8_t failed_phase=write_phase;
    uint16_t failed_index=write_data_index;
    write_first_fault=ok?0:fault;
#endif
    WRITE_PHASE(MY2K_RGB_PHASE_STOP,UINT16_MAX);
    stop();
#if MY2K_DEV10_PROFILE
    if(write_first_fault){write_phase=failed_phase;write_data_index=failed_index;}
#endif
    return ok && !fault?I2C_STATUS_SUCCESS:(fault==1?I2C_STATUS_TIMEOUT:I2C_STATUS_ERROR);
}
#if MY2K_DEV18_PROFILE
__attribute__((noipa,externally_visible))
#endif
i2c_status_t my2k_rgb_bus_write(uint8_t address,uint8_t reg,const uint8_t *data,uint16_t count,uint16_t timeout){
#if MY2K_DEV10_PROFILE
    int index=failure_index(address);
    if(index>=0)failure_cache[index].attempts++;
    write_first_fault=0;
#endif
#if MY2K_DEV4_PROFILE
    uint32_t measured=my2k_perf_clock();
#endif
    i2c_status_t result=write_transaction(address,reg,data,count,timeout);
#if MY2K_DEV10_PROFILE
    if(index>=0 && result!=I2C_STATUS_SUCCESS){
        my2k_rgb_bus_failure_diagnostics *d=&failure_cache[index];
        uint8_t cause=write_first_fault?write_first_fault:fault;
        uint32_t now=timer_read32();
        if(!d->failures){
            d->first_ms=now;d->first_register=reg;d->first_fault=cause;
            d->first_phase=write_phase;d->first_data_index=write_data_index;d->first_status=result;
        }
        d->failures++;d->last_ms=now;d->last_register=reg;d->last_fault=cause;
        d->last_phase=write_phase;d->last_data_index=write_data_index;d->last_status=result;
    }
#endif
#if MY2K_DEV4_PROFILE
    my2k_perf_record(MY2K_PERF_BUS,measured);
#endif
#if MY2K_DEV18_PROFILE
    my2k_rgb_trace_record(false,address,reg,data,count,result,write_first_fault?write_first_fault:fault,result?write_phase:0,(uintptr_t)__builtin_return_address(0),timer_read32());
#endif
    return result;
}
uint8_t my2k_rgb_bus_lines(void) { return (my2k_bus_read_scl()?1:0)|(my2k_bus_read_sda()?2:0); }
uint8_t my2k_rgb_bus_fault(void) { return fault; }
uint32_t my2k_rgb_bus_recoveries(void) { return recoveries; }
#if MY2K_DEV3_PROFILE
static i2c_status_t read_transaction(uint8_t address,uint8_t reg,uint8_t *out,uint16_t count,uint16_t timeout) {
    READ_PHASE(1);
    /*3236 is write-only. Only known readable LED controllers are accepted. */
    if ((address!=0x6e && address!=0x68 && address!=0xe8 && address!=0xee && address!=0xea) || !out || !count || count>16 || !timeout) { fault=4;return I2C_STATUS_ERROR; }
    fault=0;started=timer_read32();deadline=timeout;my2k_bus_sda(true);
    if(!raise_clock())return I2C_STATUS_TIMEOUT;
    if(!my2k_bus_read_sda()){fault=2;return I2C_STATUS_ERROR;}
    wait_us(2);my2k_bus_sda(false);wait_us(2);my2k_bus_scl(false);
    READ_PHASE(2);bool ok=byte(address);
    if(ok){READ_PHASE(3);ok=byte(reg);}
    if(ok){
        READ_PHASE(4);my2k_bus_sda(true);wait_us(2);ok=raise_clock();wait_us(2);
        my2k_bus_sda(false);wait_us(2);my2k_bus_scl(false);
        if(ok){READ_PHASE(5);ok=byte(address|1u);}
    }
    for(unsigned n=0;ok && n<count;n++) {
        READ_PHASE(6);uint8_t value=0;my2k_bus_sda(true);
        for(unsigned bit=0;bit<8;bit++) {
            wait_us(2);if(!raise_clock()){ok=false;break;}
            wait_us(2);value=(uint8_t)((value<<1)|my2k_bus_read_sda());my2k_bus_scl(false);
        }
        if(!ok)break;
        READ_PHASE(7);out[n]=value;my2k_bus_sda(n+1==count);wait_us(2);
        ok=raise_clock();wait_us(2);my2k_bus_scl(false);my2k_bus_sda(true);
        if((uint32_t)(timer_read32()-started)>=deadline){fault=1;ok=false;}
    }
#if MY2K_DEV18_PROFILE
    uint8_t earlier_phase=read_phase,earlier_fault=fault;
#endif
    READ_PHASE(8);stop();
#if MY2K_DEV18_PROFILE
    if(earlier_fault){read_phase=earlier_phase;read_first_fault=earlier_fault;}
#endif
    return ok && !fault?I2C_STATUS_SUCCESS:(fault==1?I2C_STATUS_TIMEOUT:I2C_STATUS_ERROR);
}
__attribute__((noipa,externally_visible))
i2c_status_t my2k_rgb_bus_read(uint8_t address,uint8_t reg,uint8_t *out,uint16_t count,uint16_t timeout){
    #if MY2K_DEV18_PROFILE
    read_first_fault=0;
    #endif
    i2c_status_t result=read_transaction(address,reg,out,count,timeout);
#if MY2K_DEV18_PROFILE
    my2k_rgb_trace_record(true,address,reg,out,count,result,read_first_fault?read_first_fault:fault,result?read_phase:0,(uintptr_t)__builtin_return_address(0),timer_read32());
#endif
    return result;
}
#endif
