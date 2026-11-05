// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#define CFG_UF2_FLASH_SIZE (256*1024)
#define BOARD_FLASH_APP_START 0x08010000U
#define BOARD_FLASH_APP_END 0x08040000U
#define BOARD_FLASH_SIZE (256*1024)
#define BOARD_UF2_FAMILY_ID 0x57755a57U
#define BPB_SECTOR_SIZE 512
#define HAL_FLASH_ERROR_NONE 0
#define TUF2_LOG1_HEX(x) ((void)0)
static bool prox_write_failed,fail_transfer,corrupt_transfer;
static uint16_t density=256;
#define FLASHSIZE_BASE ((uintptr_t)&density)
static unsigned writes,unlocks,checks,resets;
int ec_prox_flash_failed(void){return prox_write_failed;}
static int HAL_FLASH_Unlock(void){unlocks++;return 0;}
static int HAL_FLASH_Lock(void){return 0;}
static int HAL_FLASH_GetError(void){return 0;}
static void NVIC_SystemReset(void){resets++;}
static void flash_write(uint32_t a,const uint8_t *p,int n){
 writes++;
 if(fail_transfer){prox_write_failed=true;return;}
 if(!corrupt_transfer)memcpy((void*)(uintptr_t)a,p,(size_t)n);
}
static void board_flash_flush(void){}
#define CHECK(x) do { checks++; assert(x); } while(0)

// Board-local boot diagnostics mocks; reading them must not change their state.
static struct { uint32_t CSR; } host_rcc;
#define RCC (&host_rcc)
uint32_t _board_dfu_dbl_tap[1];
char infoUf2File[384] = "TinyUF2 Virgo boot diagnostics\r\nFlash Size: 0x";
static unsigned dfu_inits;
void virgo_port_dfu_init(void) { ++dfu_inits; }
