/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Independent register-level implementation. Register facts: ST CMSIS F722.
 * Configuration facts: investigation AUX clock/MSP evidence; no binary code copied.
 * Data cache remains disabled; the optional instruction cache is enabled in
 * aux_main. PIO buffers may reside in DTCM without DMA coherency maintenance. */
#include "aux_platform.h"
#include "aux_ring.h"
#include "aux_work.h"
#define R32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define RCC 0x40023800u
#define PWR 0x40007000u
#define FLASH 0x40023c00u
#define UART 0x40004400u
#define MY2K_BAUD 115200u
#define GPIOA 0x40020000u
static volatile uint32_t milliseconds;
/* Alias, not a second clock or allocation; preserves the debugger/model name. */
extern volatile uint32_t aux_tick_ms __attribute__((alias("milliseconds")));
static aux_ring rx,tx;
volatile uint32_t aux_uart_rx_overflows,aux_uart_tx_overflows;
volatile uint32_t aux_fault_code;
static bool wait_bits(uint32_t a,uint32_t mask,uint32_t value){
    for(uint32_t i=0;i<4000000u;i++) if((R32(a)&mask)==value)return true;
    return false;
}
bool aux_clock_init(void){
    /* RM0431 34.16.1/34.16.3: WFI gates HCLK unless DBG_SLEEP is set.
     * ASTRO's onboard SWD programmer must attach to this running target;
     * without it, idle-time AP reads can be stale before flash even starts.
     * Set only Sleep support, preserving trace/Stop/Standby configuration.
     * This is volatile AUX state; the Main bootloader remains untouched. */
    R32(0xe0042004u)|=1u;
    __asm__ volatile("dsb\nisb" ::: "memory");
    R32(0xe000e010u)=0;
    R32(0xe000ed88u)|=0xfu<<20; /* CPACR: enable FPU before softfp/library use. */
    R32(0xe000ed08u)=0x08000000u;
    __asm__ volatile("dsb\nisb" ::: "memory");
    R32(RCC+0x40)|=1u<<28; (void)R32(RCC+0x40);
    R32(PWR)|=3u<<14;
    R32(RCC)|=1u;
    if(!wait_bits(RCC,2u,2u))return false;
    R32(RCC+8)&=~3u;
    if(!wait_bits(RCC+8,3u<<2,0))return false;
    R32(RCC)&=~(1u<<24);
    if(!wait_bits(RCC,1u<<25,0))return false;
    R32(RCC)|=1u<<16;
    if(!wait_bits(RCC,1u<<17,1u<<17))return false;
    /* 16MHz HSE /16 *432 /2 =216MHz; PLLQ9 produces48MHz. */
    R32(RCC+4)=16u|(432u<<6)|(1u<<22)|(9u<<24);
    R32(RCC)|=1u<<24;
    if(!wait_bits(RCC,1u<<25,1u<<25))return false;
    R32(PWR)|=1u<<16;
    if(!wait_bits(PWR+4,1u<<16,1u<<16))return false;
    R32(PWR)|=1u<<17;
    if(!wait_bits(PWR+4,1u<<17,1u<<17))return false;
    R32(FLASH)=(R32(FLASH)&~15u)|7u; /* Seven wait states at scale1/216MHz. */
    if(!wait_bits(FLASH,15u,7u))return false;
    R32(RCC+8)=(R32(RCC+8)&~((15u<<4)|(7u<<10)|(7u<<13)|3u))|
        (5u<<10)|(4u<<13)|2u; /* AHB/1, APB1/4, APB2/2, PLL. */
    if(!wait_bits(RCC+8,3u<<2,2u<<2))return false;
    R32(0xe000e014u)=216000u-1u;
    R32(0xe000e018u)=0;
    R32(0xe000e010u)=7u;
    return true;
}
void SysTick_Handler(void){milliseconds++;}
uint32_t aux_millis(void){return milliseconds;}
bool aux_delay(uint32_t ms){
 uint32_t t=aux_millis(),last=t;unsigned stagnant=0;
 while((uint32_t)(aux_millis()-t)<ms){
  aux_work_checkpoint();uint32_t now=aux_millis();
  if(now!=last){last=now;stagnant=0;}
  else if(++stagnant==200000u){aux_fault_code=10;return false;}
 }
 return true;
}
void aux_idle(void){__asm__ volatile("wfi");}
void aux_uart_init(void){
    aux_ring_init(&rx);aux_ring_init(&tx);
    R32(RCC+0x30)|=1u;R32(RCC+0x40)|=1u<<17;(void)R32(RCC+0x40);
    R32(GPIOA)=(R32(GPIOA)&~(15u<<4))|(10u<<4); /* PA2/PA3 alternate functions. */
    R32(GPIOA+4)&=~(3u<<2);
    R32(GPIOA+8)|=15u<<4;
    R32(GPIOA+0x0c)&=~(15u<<4);
    R32(GPIOA+0x20)=(R32(GPIOA+0x20)&~(255u<<8))|(0x77u<<8);
    R32(UART)=0;R32(UART+8)=0;
    R32(UART+0x0c)=(54000000u+MY2K_BAUD/2u)/MY2K_BAUD;
    R32(UART)=1u|(1u<<2)|(1u<<3)|(1u<<5); /* UE,RE,TE,RXNEIE;8N1,oversampling16. */
    *(volatile uint8_t*)(uintptr_t)(0xe000e400u+38u)=0x40;
    R32(0xe000e104u)=1u<<6;
}
void USART2_Handler(void){
    uint32_t status=R32(UART+0x1c);
    if(status&15u){R32(UART+0x20)=15u;aux_fault_code=0x200u|(status&15u);}
    if(status&(1u<<5)){
        uint8_t byte=(uint8_t)R32(UART+0x24);
        if(!aux_ring_push(&rx,&byte,1)){aux_uart_rx_overflows++;aux_fault_code=0x210u;}
    }
    if((status&(1u<<7))&&(R32(UART)&(1u<<7))){
        uint8_t byte;
        if(aux_ring_pop(&tx,&byte))R32(UART+0x28)=byte;
        else R32(UART)&=~(1u<<7); /* Disable TXEIE when drained; hardware finishes the last byte. */
    }
}
bool aux_uart_get(uint8_t *byte){return aux_ring_pop(&rx,byte);}
bool aux_uart_rx_pending(void){return aux_ring_count(&rx)!=0;}
bool aux_uart_send(const char *bytes,size_t size){
    if(!aux_ring_push(&tx,(const uint8_t *)bytes,size)){aux_uart_tx_overflows++;return false;}
    if(size)R32(UART)|=1u<<7; /* Publish the whole frame, then enable TXE interrupt. */
    return true; /* Accepted for transmission, not a claim the remote endpoint received it. */
}
/* AUX global init08007618: GPIOB maskc000 AF12 push-pull/no pulls;
 * RCC AHB1 USB_OTG_HS bit29. Embedded FS PHY, not external ULPI. */
void aux_usb_platform_init(void){
    const uint32_t gpio=0x40020400u;
    R32(RCC+0x30)|=(1u<<1)|(1u<<29);(void)R32(RCC+0x30);
    R32(gpio)=(R32(gpio)&~(15u<<28))|(10u<<28);
    R32(gpio+4)&=~(3u<<14);R32(gpio+8)|=15u<<28;
    R32(gpio+12)&=~(15u<<28);
    R32(gpio+0x24)=(R32(gpio+0x24)&~(255u<<24))|(0xccu<<24);
    *(volatile uint8_t*)(uintptr_t)(0xe000e400u+77u)=0x80;
}

/* Fail-stop for initialization or runtime DCD failure, including ISR context.
 * No wait, retry, core reset, or mutation of worker-owned data. */
void aux_usb_platform_abort(void){
 R32(0xe000e188u)=1u<<13; /* NVIC IRQ77 disable. */
 R32(0xe000e288u)=1u<<13; /* Clear its pending IRQ. */
 R32(0x40040008u)&=~1u;  /* Global DWC2 interrupt mask. */
 R32(0x40040018u)=0;
 R32(0x40040804u)|=2u;   /* DCTL soft disconnect. */
 __asm volatile("dsb sy\nisb sy":::"memory"); /* CMSIS NVIC disable ordering. */
}
