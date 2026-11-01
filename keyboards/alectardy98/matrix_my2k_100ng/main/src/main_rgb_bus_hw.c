/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Board-local hardware I2C1 transport. Stock establishes PB6/PB7 AF4,
 * 400kHz duty2 and DMA1 RX5/TX6; the shared HAL remains unmodified.
 * No automatic GPIO fallback, retransmission or LED configuration repair. */
#include "main_rgb_bus.h"
#include "main_rgb_trace.h"
#if MY2K_DEV21_PROFILE
#include "main_rgb_first.h"
#endif
#include "main_perf.h"
#include "main_input_io.h"
#include <string.h>
#ifndef MY2K_RGB_HW_HOST
#include "hal.h"
#include "timer.h"
#else
#include "rgb_hw_hal.h"
#endif

static const I2CConfig bus_config = {OPMODE_I2C,400000,FAST_DUTY_CYCLE_2};
static const uint8_t addresses[6] = {0x6e,0x68,0xe8,0xee,0xea,0x78};
static bool configured,active;
static uint8_t fault;
static uint8_t tx_buffer[65],rx_buffer[16];
/* Page selection is a software shadow only; successful return is not wire proof. */
static uint8_t pages[3],page_known,shutdown_known,shutdown_value;
static my2k_rgb_bus_failure_diagnostics failures[6];
typedef struct {uint32_t writes,reads,ok,errors,timeouts,hal_errors,max_us;} chip_stats;
static chip_stats chips[6];
static uint32_t calls,errors,init_count,restarts,last_us,max_us,last_hal_errors;
static uint32_t cr2,ccr,trise,gpio_mode,gpio_af,gpio_type;
static volatile bool dma_failed;
static volatile uint32_t dma_failures;
void my2k_rgb_bus_dma_error(void){dma_failed=true;dma_failures++;}

static int index_for(uint8_t address){
    for(unsigned i=0;i<6;i++)if(addresses[i]==address)return (int)i;
    return -1;
}
static bool function_range(uint8_t reg,uint16_t count){
    if((unsigned)reg+count>13)return false;
    for(unsigned i=reg;i<(unsigned)reg+count;i++)if(i==4||i==7)return false;
    return true;
}
static bool readable(int index,uint8_t reg,uint16_t count){
    if(index<0||index==5)return false; /*3236 is write-only.*/
    if(index<2)return true;
    unsigned c=(unsigned)index-2;
    if(!(page_known&(1u<<c)))return false;
    if(pages[c]==11)return function_range(reg,count);
    /* Frame RAM reads only while explicitly known to be in software shutdown.
     * No operation here enters shutdown just to make a read possible. */
    return pages[c]<8&&(shutdown_known&(1u<<c))&&!(shutdown_value&(1u<<c))&&
           (unsigned)reg+count<=0xb4;
}
void my2k_rgb_bus_init(void){
    if(configured)return; /* Included QMK family initializers also call this. */
    configured=true;init_count++;
    palSetLineMode(PAL_LINE(GPIOB,6),PAL_MODE_ALTERNATE(4)|PAL_STM32_OTYPE_OPENDRAIN|PAL_STM32_OSPEED_HIGHEST);
    palSetLineMode(PAL_LINE(GPIOB,7),PAL_MODE_ALTERNATE(4)|PAL_STM32_OTYPE_OPENDRAIN|PAL_STM32_OSPEED_HIGHEST);
    i2cStart(&I2CD1,&bus_config);
    /* Never read SR1/SR2 for diagnostics: their order has protocol side effects. */
    cr2=I2CD1.i2c->CR2;ccr=I2CD1.i2c->CCR;trise=I2CD1.i2c->TRISE;
    gpio_mode=GPIOB->MODER;gpio_af=GPIOB->AFRL;gpio_type=GPIOB->OTYPER;
}
static void remember_write(int index,uint8_t reg,const uint8_t *data,uint16_t count,bool ok){
    if(index<2||index>4)return;
    unsigned c=(unsigned)index-2;uint8_t bit=1u<<c;
    if(!ok){page_known&=~bit;shutdown_known&=~bit;return;}
    if(reg==0xfd&&count==1){pages[c]=data[0];page_known|=bit;}
    else if((page_known&bit)&&pages[c]==11&&reg<=10&&(unsigned)reg+count>10){
        shutdown_known|=bit;
        if(data[10-reg]&1)shutdown_value|=bit;else shutdown_value&=~bit;
    }
}
static i2c_status_t transfer(bool read,uint8_t address,uint8_t reg,const uint8_t *input,uint8_t *output,uint16_t count,uint16_t timeout,uintptr_t caller){
    int index=index_for(address);uint8_t phase=MY2K_RGB_PHASE_VALIDATE;
    uint32_t start=my2k_perf_clock(),hal_error=0;fault=0;
    bool valid=index>=0&&count&&count<=(read?16:64)&&timeout&&timeout<=100&&
               (read?output!=NULL:input!=NULL);
    if(read&&valid)valid=readable(index,reg,count);
    i2c_status_t result=I2C_STATUS_ERROR;
#if MY2K_DEV21_PROFILE
    uint8_t selected=(index>=2&&index<=4&&(page_known&(1u<<(index-2))))?pages[index-2]:0xff;
    my2k_first_event(FIRST_IO_BEGIN,address>>1,selected,reg,count,read,0,(uint32_t)caller,(!read&&valid)?input:0);
    if(!read&&valid&&input&&count>8&&count<=64){uint32_t hash=2166136261u;for(unsigned j=0;j<count;j++)hash=(hash^input[j])*16777619u;my2k_first_event(FIRST_PAYLOAD_HASH,address>>1,selected,reg,count,0,0,hash,0);}
#endif
    if(!valid){fault=4;goto done;}
    /* APIs are task-context only. No nested scan/checkpoint while owning a page. */
    if(active||__get_IPSR()||__get_PRIMASK()||__get_BASEPRI()){fault=7;goto done;}
    active=true;my2k_rgb_bus_init();
    tx_buffer[0]=reg;
    if(!read)memcpy(tx_buffer+1,input,count);
    if(read)chips[index].reads++;else{chips[index].writes++;failures[index].attempts++;}
    i2cAcquireBus(&I2CD1);
    if(I2CD1.state==I2C_STOP){i2cStart(&I2CD1,&bus_config);restarts++;}
    /* The HAL performs START/address/pointer/repeated START/read and last-byte
     * NACK/STOP with DMA. Buffer ownership lasts through completion/abort. */
    dma_failed=false;
    phase=9; /* Hardware transfer: no invented address-vs-data NACK precision. */
    bool input_window=my2k_input_io_begin();
    msg_t status=i2cMasterTransmitTimeout(&I2CD1,address>>1,tx_buffer,read?1:count+1,
                                        read?rx_buffer:NULL,read?count:0,TIME_MS2I(timeout));
    if(input_window)my2k_input_io_end();
    hal_error=i2cGetErrors(&I2CD1);
    if(dma_failed){hal_error|=0x80000000u;status=MSG_RESET;}
    if(status==MSG_OK){result=I2C_STATUS_SUCCESS;if(read)memcpy(output,rx_buffer,count);chips[index].ok++;}
    else{
        fault=dma_failed?8:status==MSG_TIMEOUT?1:(hal_error&I2C_ACK_FAILURE)?3:
              (hal_error&I2C_ARBITRATION_LOST)?6:(hal_error&I2C_BUS_ERROR)?5:7;
        result=status==MSG_TIMEOUT?I2C_STATUS_TIMEOUT:I2C_STATUS_ERROR;
        chips[index].errors++;chips[index].timeouts+=status==MSG_TIMEOUT;
        chips[index].hal_errors|=hal_error;
        /* Abort/release MCU peripheral resources only. No LED reset, pin pulse,
         * retry of partial writes, or current/PWM/shutdown repair is performed. */
        i2cStop(&I2CD1);
    }
    i2cReleaseBus(&I2CD1);active=false;
    if(!read)remember_write(index,reg,input,count,result==I2C_STATUS_SUCCESS);
done:
    #if MY2K_DEV21_PROFILE
    my2k_first_event(FIRST_IO_END,address>>1,selected,reg,count,(uint8_t)result,fault,hal_error,read?(result?0:output):(valid?input:0));
    if(result&&phase==9)my2k_first_trigger(1,address>>1,reg);
#endif
    calls++;last_hal_errors=hal_error;last_us=(my2k_perf_clock()-start)/168u;
    if(last_us>max_us)max_us=last_us;
    if(result)errors++;
    if(index>=0){
        if(last_us>chips[index].max_us)chips[index].max_us=last_us;
        if(!read&&result){
            my2k_rgb_bus_failure_diagnostics *d=&failures[index];uint32_t now=timer_read32();
            if(!d->failures){d->first_ms=now;d->first_register=reg;d->first_fault=fault;d->first_phase=phase;d->first_data_index=UINT16_MAX;d->first_status=result;}
            d->failures++;d->last_ms=now;d->last_register=reg;d->last_fault=fault;d->last_phase=phase;d->last_data_index=UINT16_MAX;d->last_status=result;
        }
    }
    my2k_perf_record(MY2K_PERF_BUS,start);
    my2k_rgb_trace_record(read,address,reg,read?output:input,count,result,fault,result?phase:0,caller,timer_read32());
    return result;
}
__attribute__((noipa,externally_visible))
i2c_status_t my2k_rgb_bus_write(uint8_t address,uint8_t reg,const uint8_t *data,uint16_t count,uint16_t timeout){return transfer(false,address,reg,data,NULL,count,timeout,(uintptr_t)__builtin_return_address(0));}
__attribute__((noipa,externally_visible))
i2c_status_t my2k_rgb_bus_read(uint8_t address,uint8_t reg,uint8_t *data,uint16_t count,uint16_t timeout){return transfer(true,address,reg,NULL,data,count,timeout,(uintptr_t)__builtin_return_address(0));}
uint8_t my2k_rgb_bus_lines(void){return (palReadLine(PAL_LINE(GPIOB,6))?1:0)|(palReadLine(PAL_LINE(GPIOB,7))?2:0);}
uint8_t my2k_rgb_bus_fault(void){return fault;}
uint32_t my2k_rgb_bus_recoveries(void){return restarts;}
bool my2k_rgb_bus_failure(unsigned controller,my2k_rgb_bus_failure_diagnostics *out){if(!out)return false;if(controller>=6){memset(out,0,sizeof *out);return false;}*out=failures[controller];return true;}
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
__attribute__((noipa,externally_visible))
bool my2k_rgb_bus_hw_page(uint8_t page,uint8_t index,uint8_t p[32]){
    if(!p||(page!=43&&page!=44)||(page==43?index>3:index>=6))return false;
    memset(p+4,0,28);p[4]=1;p[5]=index;p[6]=configured;p[7]=active;
    uint32_t a[6];
    if(page==43){
        const uint32_t v[4][6]={{400000,cr2,ccr,trise,init_count,restarts},
                              {calls,errors,last_us,max_us,last_hal_errors,2},
                              {gpio_mode,gpio_af,gpio_type,5,6,0x1f6f},
                              {dma_failures,42,0x40005400,6,7,4}};
        memcpy(a,v[index],sizeof a);
    }else{const chip_stats*d=&chips[index];p[6]=addresses[index];p[7]=fault;
        a[0]=d->writes;a[1]=d->reads;a[2]=d->ok;a[3]=d->errors;a[4]=d->timeouts;a[5]=d->hal_errors;}
    for(unsigned i=0;i<6;i++)put(p+8+4*i,a[i]);
    return true;
}
