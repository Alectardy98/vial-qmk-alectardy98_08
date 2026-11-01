/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SD native bus PIO. Legacy profiles never emit writes; stock-parity adds
 * bounded single-blockCMD24 only, never erase/lock/format commands.
 * GPIO/SDMMC facts from ST headers and AUX behavioral pin specification. */
#if MY2K_DEV22_PROFILE
#include "aux_perf22.h"
#endif
#include "aux_platform.h"
#include "aux_work.h"
#ifndef R32
#define R32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#endif
#define RCC 0x40023800u
#define SD 0x40012c00u
#define CMD_ERROR ((1u<<0)|(1u<<2))
#define DATA_ERROR ((1u<<1)|(1u<<3)|(1u<<4)|(1u<<5)|(1u<<9))
#define R1_ERROR 0xfdffe008u
static uint32_t sector_count;
static uint16_t card_rca;
static bool high_capacity;
/* Only this worker issues card commands. A successful post-transfer CMD13
 * remains valid until the next command; do not ask it twice back-to-back.
 * Every command attempt invalidates the observation, including failures. */
static bool transfer_ready_known;
volatile uint32_t aux_sd_control_write_failures;
static bool control_write(unsigned offset,uint32_t value){
#if AUX_SD_REGISTER_FENCE
    /* RM0431Rev4 §§30.8.1/2/4/9: repeated control-register writes need
     * 3 SDMMCCLK +2 PCLK2 periods. At216/48/108MHz this is17.5core
     * cycles. Readback/DSB drains the posted APB write before18DWTcycles.
     * This is peripheral timing, independent of compiler/cache/layout. */
    if(!(R32(0xe0001000u)&1u)){aux_sd_control_write_failures++;return false;}
#endif
    R32(SD+offset)=value;
#if AUX_SD_REGISTER_FENCE
    (void)R32(SD+offset);__asm__ volatile("dsb" ::: "memory");
    uint32_t start=R32(0xe0001004u);
    for(unsigned n=0;n<128u;n++)if((uint32_t)(R32(0xe0001004u)-start)>=18u)return true;
    aux_sd_control_write_failures++;return false; /* stopped DWT: bounded fault */
#else
    return true;
#endif
}
#if AUX_SD_FAST_READ
volatile uint32_t aux_sd_card_csd[4],aux_sd_card_max_hz;
/* Both data directions stay within the measured CSD default-speed rating.
 * No CMD6/high-speed switch, voltage change or bypass. */
static bool data_clock(bool writing){
    if(R32(SD+0x34)&0x3800u)return false; /* CPSM/DPSM must be idle. */
    (void)writing;
    unsigned divider=aux_sd_card_max_hz>=24000000u?0u:
                     aux_sd_card_max_hz>=12000000u?2u:4u;
    uint32_t old=R32(SD+4),next=(old&~255u)|divider;
    return old==next||control_write(4,next);
}
#endif
#if MY2K_SD_BULK8
static uint32_t content_epoch;
uint32_t aux_sd_content_epoch(void){return content_epoch;}
volatile uint32_t aux_sd_bulk_attempts,aux_sd_bulk_successes,aux_sd_bulk_failures;
#if AUX_SD_READ_TRACE
/* First failed transaction, before DPSM cleanup/STOP can erase its evidence. */
volatile uint32_t aux_sd_read_trace[14];
static void read_failed(unsigned stage,uint32_t lba,unsigned count,unsigned words,uint32_t status,uint32_t reply){
    if(aux_sd_read_trace[1])return;
    aux_sd_read_trace[0]=0x31524453u;aux_sd_read_trace[2]=lba;
    aux_sd_read_trace[3]=count;aux_sd_read_trace[4]=words;
    aux_sd_read_trace[5]=status;aux_sd_read_trace[6]=reply;
    aux_sd_read_trace[7]=R32(SD+0x10);aux_sd_read_trace[8]=R32(SD+0x0c);
    aux_sd_read_trace[9]=R32(SD+0x2c);aux_sd_read_trace[10]=R32(SD+0x30);
    aux_sd_read_trace[11]=R32(SD+4);aux_sd_read_trace[12]=aux_millis_fast();
    aux_sd_read_trace[13]=aux_sd_bulk_attempts;aux_sd_read_trace[1]=stage;
}
#else
static void read_failed(unsigned stage,uint32_t lba,unsigned count,unsigned words,uint32_t status,uint32_t reply){
 (void)stage;(void)lba;(void)count;(void)words;(void)status;(void)reply;
}
#endif
#endif
volatile uint32_t aux_sd_init_stage,aux_sd_init_attempts;
volatile uint32_t aux_sd_command_count,aux_sd_last_command,aux_sd_last_status,aux_sd_last_response;
volatile uint32_t aux_sd_read_attempts,aux_sd_read_successes,aux_sd_read_failures,aux_sd_read_ready_queries;
volatile uint32_t aux_sd_last_read_lba,aux_sd_last_read_status,aux_sd_last_read_words,aux_sd_read_fail_stage;
/* Deliberately closed whitelist: ACMD6 and ACMD41 require CMD55 wrapper. */
static bool command(uint8_t cmd,uint32_t arg,unsigned response,bool no_crc,uint32_t *reply){
    transfer_ready_known=false;
    switch(cmd){case 0:case 2:case 3:case 6:case 7:case 8:case 9:case 13:case 16:case 17:case 41:case 55:break;
#if MY2K_SD_BULK8
        case 12:case 18:break; /* bounded read-multiple + stop; no new writes */
#endif
#if MY2K_STOCK_PARITY_PROFILE && (!MY2K_DEV7_PROFILE || MY2K_RELEASE1_PROFILE)
        case 24:case 25:break; /* bounded asset writes; no erase/lock/format */
#endif
        default:return false;}
    aux_sd_last_command=cmd;aux_sd_command_count++;aux_sd_last_status=0;aux_sd_last_response=0;
    /* A new CPSM request must never inherit a preceding completion flag.
     * Status clearing crosses the SDMMC clock domain; observe it complete
     * before launching, rather than depending on CPU/Flash/cache speed. */
    uint32_t barrier_start=aux_millis_fast();unsigned barrier_polls=0;
    while(R32(SD+0x34)&(1u<<11)){
        aux_work_checkpoint();
        if(++barrier_polls>=200000u||(uint32_t)(aux_millis_fast()-barrier_start)>=200u)return false;
    }
    R32(SD+0x38)=0xc5u; /* CMDCRC/timeout/end/sent only; retain DPSM flags. */
    barrier_polls=0;
    while(R32(SD+0x34)&0xc5u){
        aux_work_checkpoint();
        if(++barrier_polls>=200000u||(uint32_t)(aux_millis_fast()-barrier_start)>=200u)return false;
    }
    R32(SD+8)=arg;
    if(!control_write(0x0c,cmd|(response<<6)|(1u<<10)))return false;
    uint32_t start=aux_millis_fast(),status;unsigned polls=0;
    do{
        aux_work_checkpoint();
        status=R32(SD+0x34);
        if(status&(1u<<2)){aux_sd_last_status=status;return false;}
        if(!response&&(status&(1u<<7))){aux_sd_last_status=status;R32(SD+0x38)=0xc5u;return true;}
        /* STM32F7 LL SDMMC_GetCmdResp1: CMDREND/CCRCFAIL can precede
         * CMDACT deassertion. Starting the next command while CPSM is active
         * is invalid; flash/cache speed must not determine completion. */
        if(response&&(status&((1u<<6)|(1u<<0)))&&!(status&(1u<<11)))break;
    }while(++polls<200000u&&(uint32_t)(aux_millis_fast()-start)<200u);
    aux_sd_last_status=status;
    if(!(status&((1u<<6)|(1u<<0)))||(status&(1u<<11)))return false;
    if((status&1u)&&!no_crc)return false;
    /* CRC-bearing short responses include the originating command index.
     * R2 long and R3/OCR responses do not carry that same index contract. */
    if(response==1&&!no_crc&&(R32(SD+0x10)&63u)!=cmd)return false;
    if(reply){*reply=R32(SD+0x14);aux_sd_last_response=*reply;}
    R32(SD+0x38)=0xc5u;
    return true;
}
static bool app_command(uint8_t cmd,uint32_t arg,bool no_crc,uint32_t *reply){
    uint32_t r;
    if(!command(55,(uint32_t)card_rca<<16,1,false,&r)||(r&R1_ERROR))return false;
    return command(cmd,arg,1,no_crc,reply);
}
static void alternate(uint32_t port,unsigned pin){
    unsigned shift=pin*2;
    R32(port)=(R32(port)&~(3u<<shift))|(2u<<shift);
    R32(port+4)&=~(1u<<pin);
    R32(port+8)|=3u<<shift;
    R32(port+0x0c)&=~(3u<<shift); /* Stock no-pull wiring; external pull-ups required. */
    unsigned afshift=(pin%8u)*4u;uint32_t af=port+0x20+(pin/8u)*4u;
    R32(af)=(R32(af)&~(15u<<afshift))|(12u<<afshift);
}
bool aux_sd_init(void){
    aux_sd_init_attempts++;aux_sd_init_stage=1;
#if MY2K_SD_BULK8
    content_epoch++;
#endif
    sector_count=0;card_rca=0;high_capacity=false;
    R32(RCC+0x30)|=(1u<<2)|(1u<<3);
    R32(RCC+0x44)|=1u<<11;(void)R32(RCC+0x44);
    R32(RCC+0x90)&=~((1u<<27)|(1u<<28)); /* PLLQ48 selected for CK48/SDMMC1. */
    for(unsigned p=8;p<=12;p++)alternate(0x40020800u,p);
    alternate(0x40020c00u,2);
    if(!control_write(0,3)||!control_write(4,(1u<<8)|118u))return false; /* 400kHz identification. */
    if(!aux_delay(2))return false;
    aux_sd_init_stage=2;
    if(!command(0,0,0,false,0))return false;
    uint32_t r=0;
    aux_sd_init_stage=3;
    bool v2=command(8,0x1aau,1,false,&r);
    if(v2&&(r&0xfffu)!=0x1aau)return false;
    uint32_t start=aux_millis_fast();unsigned attempts=0;
    aux_sd_init_stage=4;
    do{
        if(!app_command(41,0x00ff8000u|(v2?(1u<<30):0u),true,&r))return false;
        if(r&(1u<<31))break;
        if(!aux_delay(10))return false;
    }while(++attempts<200u&&(uint32_t)(aux_millis_fast()-start)<2000u);
    if(!(r&(1u<<31)))return false;
    high_capacity=(r&(1u<<30))!=0;
    aux_sd_init_stage=5;
    if(!command(2,0,3,false,0)||!command(3,0,1,false,&r))return false;
    card_rca=(uint16_t)(r>>16);
    if(!card_rca||!command(9,(uint32_t)card_rca<<16,3,false,0))return false;
    uint32_t csd[4];for(unsigned n=0;n<4;n++)csd[n]=R32(SD+0x14+4u*n);
#if AUX_SD_FAST_READ
    for(unsigned n=0;n<4;n++)aux_sd_card_csd[n]=csd[n];
    /* Only explicitly documented default-speed encodings enable acceleration.
     * Unknown/reserved values stay conservative; never infer from capacity. */
    aux_sd_card_max_hz=(csd[0]&255u)==0x32u?25000000u:
                       (csd[0]&255u)==0x5au?50000000u:0u;
#endif
    uint32_t count;
    aux_sd_init_stage=6;
    if(!aux_csd_sectors(csd,&count))return false;
    aux_sd_init_stage=7;
    if(!command(7,(uint32_t)card_rca<<16,1,false,&r)||(r&R1_ERROR))return false;
    if(!high_capacity&&(!command(16,512,1,false,&r)||(r&R1_ERROR)))return false;
    aux_sd_init_stage=8;
    if(!app_command(6,2,false,&r)||(r&R1_ERROR))return false;
    /* Stock handle08005FA0 +0x14=0x4000, forwarded by0800F498/0800F9D4.
     * Preserve hardware flow control when yielding between PIO FIFO bursts. */
    uint32_t initial_divider=3u;
#if AUX_SD_FAST_READ
    initial_divider=aux_sd_card_max_hz>=12000000u?2u:4u;
#endif
    if(!control_write(4,(1u<<14)|(1u<<8)|(1u<<11)|initial_divider))return false;
#if AUX_SD_FAST_READ
    if(!data_clock(false))return false;
#endif
    sector_count=count;aux_sd_init_stage=9;return true;
}
uint32_t aux_sd_sectors(void){return sector_count;}
#if MY2K_DEV7_PROFILE
static bool wait_transfer_state(void){
    /* Stock0800601C waits08005F5C/0800F5AC after DATAEND until CMD13
     * reports TRAN4. DATAEND alone is not the next-command readiness barrier.
     * Only the existing status command is retried; no reset/clock/card mutation.
     * Query cap remains finite even if SysTick stops. */
    /* Reuse only a confirmed TRAN state with both command and data machines
     * idle. Data completion still goes through the original CMD13 barrier. */
    if(transfer_ready_known&&!(R32(SD+0x34)&0x3800u))return true;
    transfer_ready_known=false;
    uint32_t start=aux_millis_fast();
    for(unsigned attempts=0;attempts<4096u;attempts++){
        uint32_t r;aux_sd_read_ready_queries++;
        if(!command(13,(uint32_t)card_rca<<16,1,false,&r)||(r&R1_ERROR))return false;
        if(((r>>9)&15u)==4u){transfer_ready_known=true;return true;}
        if((uint32_t)(aux_millis_fast()-start)>=500u)return false;
    }
    return false;
}
#endif
#if MY2K_DEV7_PROFILE
static bool read_sector(uint32_t lba,uint8_t block[512]){
#else
bool aux_sd_read(uint32_t lba,uint8_t block[512]){
#endif
    if(!block||lba>=sector_count||(!high_capacity&&lba>UINT32_MAX/512u))return false;
    uint32_t reply;
#if MY2K_DEV7_PROFILE
    aux_sd_read_fail_stage=2;
    if(!wait_transfer_state())return false;
#else
    if(!command(13,(uint32_t)card_rca<<16,1,false,&reply)||
       (reply&R1_ERROR)||((reply>>9)&15u)!=4u)return false;
#endif
#if AUX_SD_FAST_READ
    if(!data_clock(false))return false;
#endif
    if(!control_write(0x2c,0))return false;
    R32(SD+0x38)=0x5ffu;
    R32(SD+0x24)=4800000u;R32(SD+0x28)=512;
    if(!control_write(0x2c,(9u<<4)|(1u<<1)|1u))return false;
#if MY2K_DEV7_PROFILE
    aux_sd_read_fail_stage=3;
#endif
    if(!command(17,high_capacity?lba:lba*512u,1,false,&reply)||(reply&R1_ERROR)){
        (void)control_write(0x2c,0);return false;
    }
    unsigned words=0,polls=0;uint32_t start=aux_millis_fast(),status;
    do{
        aux_work_checkpoint();
        status=R32(SD+0x34);
        if(status&DATA_ERROR)break;
        if((status&(1u<<21))&&words<128u){
            /* RXFIFOHF documents at least8 words ready. Drain only that
             * bounded batch before the next cooperative checkpoint. The
             * single-word tail, DATAEND/CRC checks and all timeouts remain. */
            unsigned available=(status&(1u<<15))?8u:1u;
            if(available>128u-words)available=128u-words;
            for(unsigned n=0;n<available;n++,words++){
                uint32_t word=R32(SD+0x80);
                for(unsigned b=0;b<4;b++)block[words*4u+b]=(uint8_t)(word>>(8u*b));
            }
        }
        if((status&(1u<<8))&&words==128u)break;
    }while(++polls<500000u&&(uint32_t)(aux_millis_fast()-start)<500u);
#if MY2K_DEV7_PROFILE
    aux_sd_last_read_status=status;aux_sd_last_read_words=words;aux_sd_read_fail_stage=4;
#endif
    bool stopped=control_write(0x2c,0);R32(SD+0x38)=0x5ffu;
    bool okay=stopped&&words==128u&&(status&(1u<<8))&&!(status&DATA_ERROR);
#if MY2K_DEV7_PROFILE
    if(!okay)return false;
    aux_sd_read_fail_stage=5;
    if(!wait_transfer_state())return false;
    aux_sd_read_successes++;aux_sd_read_fail_stage=0;
#endif
    return okay;
}
#if MY2K_DEV7_PROFILE
bool aux_sd_read(uint32_t lba,uint8_t block[512]){
    aux_sd_read_attempts++;aux_sd_last_read_lba=lba;
    aux_sd_last_read_status=aux_sd_last_read_words=0;aux_sd_read_fail_stage=1;
#if MY2K_DEV22_PROFILE
    aux_perf22_count(PERF22_SD_CALL);bool scope=aux_perf22_enter(PERF22_SD);
#endif
    bool okay=read_sector(lba,block);
#if MY2K_DEV22_PROFILE
    aux_perf22_leave(scope);
#endif
    if(!okay)aux_sd_read_failures++;
    return okay;
}
#endif
#if MY2K_SD_BULK8
/* Bounded CMD18/R1b CMD12 path, using the existing4-bit9.6MHz bus. The
 * single-sector implementation remains the compatibility/reference path. */
bool aux_sd_read_blocks(uint32_t lba,uint8_t *out,unsigned count){
    if(!out||!count||count>8u||lba>=sector_count||count>sector_count-lba||
       (!high_capacity&&lba>UINT32_MAX/512u))return false;
    if(count==1u)return aux_sd_read(lba,out);
    aux_sd_bulk_attempts++;aux_sd_read_attempts+=count;aux_sd_last_read_lba=lba;
    aux_sd_last_read_words=0;aux_sd_read_fail_stage=2;
#if MY2K_DEV22_PROFILE
    aux_perf22_count(PERF22_SD_CALL);bool scope=aux_perf22_enter(PERF22_SD);
#endif
    uint32_t reply=0,status=0;unsigned words=0,polls=0;bool started=false,okay=false;
    if(!wait_transfer_state()){read_failed(2,lba,count,0,R32(SD+0x34),aux_sd_last_response);goto done;}
#if AUX_SD_FAST_READ
    if(!data_clock(false)){read_failed(1,lba,count,0,R32(SD+0x34),reply);goto done;}
#endif
    if(!control_write(0x2c,0)){read_failed(1,lba,count,0,R32(SD+0x34),reply);goto done;}R32(SD+0x38)=0x5ffu;
    R32(SD+0x24)=4800000u;R32(SD+0x28)=512u*count;
    if(!control_write(0x2c,(9u<<4)|(1u<<1)|1u)){read_failed(1,lba,count,0,R32(SD+0x34),reply);goto stop;}aux_sd_read_fail_stage=3;
    started=true; /* Even an ambiguous CMD18 response must be stopped. */
    if(!command(18,high_capacity?lba:lba*512u,1,false,&reply)||(reply&R1_ERROR)){read_failed(3,lba,count,0,R32(SD+0x34),reply);goto stop;}
    uint32_t start=aux_millis_fast();unsigned total=count*128u;
    do{
        aux_work_checkpoint();status=R32(SD+0x34);
        if(status&DATA_ERROR)break;
        unsigned available=(status&(1u<<15))?8u:
            ((status&(1u<<8))&&(status&(1u<<21)))?1u:0u;
        if(available>total-words)available=total-words;
        for(unsigned i=0;i<available;i++,words++){
            uint32_t word=R32(SD+0x80);
            for(unsigned b=0;b<4;b++)out[4u*words+b]=(uint8_t)(word>>(8u*b));
        }
        if((status&(1u<<8))&&words==total)break;
    }while(++polls<500000u*count&&(uint32_t)(aux_millis_fast()-start)<500u);
    aux_sd_read_fail_stage=4;
    okay=words==total&&(status&(1u<<8))&&!(status&DATA_ERROR);
    if(!okay)read_failed(4,lba,count,words,status,reply);
stop:
    aux_sd_last_read_status=status;aux_sd_last_read_words=words;
    if(!control_write(0x2c,0))okay=false;
    R32(SD+0x38)=0x5ffu;
    if(started){
        bool stopped=command(12,0,1,false,&reply)&&!(reply&R1_ERROR);
        if(!stopped){read_failed(5,lba,count,words,R32(SD+0x34),reply);okay=false;}
        else if(!wait_transfer_state()){read_failed(6,lba,count,words,R32(SD+0x34),aux_sd_last_response);okay=false;}
    }
done:
    if(okay){aux_sd_bulk_successes++;aux_sd_read_successes+=count;aux_sd_read_fail_stage=0;}
    else{aux_sd_bulk_failures++;aux_sd_read_failures++;}
#if MY2K_DEV22_PROFILE
    aux_perf22_leave(scope);
#endif
    return okay;
}
#endif
#if MY2K_STOCK_PARITY_PROFILE && (!MY2K_DEV7_PROFILE || MY2K_RELEASE1_PROFILE)
#if AUX_SD_WRITE_TRACE
/* First failure is published last, then frozen. Probe reads require no halt
 * during I/O. Never retries or changes card/controller recovery behavior. */
volatile uint32_t aux_sd_write_trace[12];
static bool write_failed(unsigned stage,uint32_t lba,uint32_t status,unsigned words,uint32_t reply){
    if(!aux_sd_write_trace[3]){
        aux_sd_write_trace[4]=lba;aux_sd_write_trace[5]=status;
        aux_sd_write_trace[6]=words;aux_sd_write_trace[7]=reply;
        aux_sd_write_trace[8]=aux_sd_last_command;
        aux_sd_write_trace[9]=R32(SD+0x30);
        aux_sd_write_trace[10]=R32(SD+0x2c);
        aux_sd_write_trace[11]=aux_millis_fast();
        aux_sd_write_trace[3]=stage;
    }
    return false;
}
#else
static bool write_failed(unsigned stage,uint32_t lba,uint32_t status,unsigned words,uint32_t reply){
    (void)stage;(void)lba;(void)status;(void)words;(void)reply;return false;
}
#endif
/* SD Association Physical Layer §4.3.4 and ST F7 HAL_SD_WriteBlocks:
 * bounded CMD25/DLEN plus explicit CMD12; no CMD23/preerase or clock change.
 * The original CMD24 path is exactly count1. Only the filesystem owner calls
 * this function, and a failure is never replayed as a single-sector retry. */
__attribute__((noinline)) bool aux_sd_write_blocks(uint32_t lba,const uint8_t *block,unsigned count){
    if(!block || !count || count>8u || lba>=sector_count || count>sector_count-lba || (!high_capacity&&lba>UINT32_MAX/512u))return false;
#if AUX_SD_WRITE_TRACE
    aux_sd_write_trace[0]=0x31574453u;aux_sd_write_trace[1]+=count;
#endif
#if MY2K_SD_BULK8
    /* Invalidate local read-ahead before any possibly partial physical write. */
    content_epoch++;
#endif
    uint32_t reply=0;
#if AUX_SD_FAST_READ
    if(!data_clock(true))return write_failed(1,lba,R32(SD+0x34),0,reply);
#endif
    if(!command(13,(uint32_t)card_rca<<16,1,false,&reply) || (reply&R1_ERROR) ||
       ((reply>>9)&15u)!=4u || !(reply&(1u<<8)))return write_failed(2,lba,R32(SD+0x34),0,reply);
    if(!control_write(0x2c,0))return write_failed(1,lba,R32(SD+0x34),0,reply);
    R32(SD+0x38)=0x5ffu;R32(SD+0x24)=4800000u;R32(SD+0x28)=512u*count;
    /*512-byte/to-card/block/DPSM; retain the proven write clock/flow control. */
    if(!control_write(0x2c,(9u<<4)|1u))return write_failed(1,lba,R32(SD+0x34),0,reply);
    if(!command(count>1u?25:24,high_capacity?lba:lba*512u,1,false,&reply) || (reply&R1_ERROR)){
        (void)write_failed(3,lba,R32(SD+0x34),0,reply);
        (void)control_write(0x2c,0);
        if(count>1u)(void)command(12,0,1,false,&reply);
        return false;
    }
    unsigned total=count*128u,words=0,polls=0;uint32_t start=aux_millis_fast(),status=0;
    do{
        aux_work_checkpoint();status=R32(SD+0x34);
        if(status&(DATA_ERROR|(1u<<8)))break;
        if((status&(1u<<14)) && words<total){
            for(unsigned burst=0;burst<8 && words<total;burst++,words++){
                const uint8_t *p=block+4u*words;
                R32(SD+0x80)=(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
            }
        }
        if(status&(1u<<8))break;
    }while(++polls<500000u*count && (uint32_t)(aux_millis_fast()-start)<500u*count);
    bool okay=words==total && (status&(1u<<8)) && !(status&DATA_ERROR);
    if(!okay){(void)write_failed(4,lba,status,words,reply);(void)control_write(0x2c,0);}
    if(count>1u&&(!command(12,0,1,false,&reply)||(reply&R1_ERROR))){(void)write_failed(7,lba,status,words,reply);okay=false;}
    if(!control_write(0x2c,0))okay=false;
    R32(SD+0x38)=0x5ffu;
    if(!okay)return false;
    /* DATAEND is not card programming completion. Wait until CMD13 reports
     * transfer state+READY_FOR_DATA before reporting successful physical I/O. */
    start=aux_millis_fast();
    for(unsigned attempts=0;attempts<512 && (uint32_t)(aux_millis_fast()-start)<500u;attempts++){
        if(!command(13,(uint32_t)card_rca<<16,1,false,&reply) || (reply&R1_ERROR))return write_failed(5,lba,R32(SD+0x34),words,reply);
        if(((reply>>9)&15u)==4u && (reply&(1u<<8))){
#if AUX_SD_WRITE_TRACE
            aux_sd_write_trace[2]+=count;
#endif
            return true;
        }
        if(!aux_delay(1))return write_failed(6,lba,R32(SD+0x34),words,reply);
    }
    return write_failed(6,lba,R32(SD+0x34),words,reply);
}
bool aux_sd_write(uint32_t lba,const uint8_t block[512]){return aux_sd_write_blocks(lba,block,1);}
#endif
