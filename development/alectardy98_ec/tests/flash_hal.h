// SPDX-License-Identifier: GPL-3.0-or-later
// Host-only ChibiOS EFL model; never included by firmware.

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
static inline void chSysHalt(const char *m){(void)m;abort();}
typedef uint32_t flash_offset_t;
typedef uint32_t flash_sector_t;
typedef int flash_error_t;
#define FLASH_NO_ERROR 0
#define FLASH_ERROR_UNIMPLEMENTED 11
#define HAL_RET_SUCCESS 0
#define FLASH_BUSY_ERASING 1
#define FLASH_ATTR_ERASED_IS_ONE 1
#define FLASH_ATTR_MEMORY_MAPPED 2
typedef struct { uint32_t offset,size; } flash_sector_descriptor_t;
typedef struct {uint32_t attributes,page_size,sectors_count; const flash_sector_descriptor_t *sectors;uint32_t sectors_size;uint8_t *address;uint32_t size;} flash_descriptor_t;
enum { FLASH_STOP,FLASH_READY,FLASH_ERASE };
typedef struct {const flash_descriptor_t *descriptor; void *flash; int state;} EFlashDriver;
typedef EFlashDriver BaseFlash;
#define FLASH ((void *)0x40023c00U)
extern EFlashDriver EFLD1;
extern unsigned char diag_arena[1024];
extern unsigned char diag_host_flash[131072];
static inline const flash_descriptor_t *flashGetDescriptor(BaseFlash *p){return p->descriptor;}
static inline uint32_t flashGetSectorSize(BaseFlash*p,unsigned i){assert(i==2);return p->descriptor->sectors[i].size;}
static inline uint32_t flashGetSectorOffset(BaseFlash*p,unsigned i){assert(i==2);return p->descriptor->sectors[i].offset;}
static inline void *flashGetOffsetAddress(BaseFlash*p,uint32_t off){(void)p;assert(off>=0x8000 && off+4<=0xc000);return diag_host_flash+off;}
int eflStart(EFlashDriver *,void*);
void eflStop(EFlashDriver *);
int flashProgram(BaseFlash*,uint32_t,size_t,const uint8_t*);
int flashStartEraseSector(BaseFlash*,unsigned);
int flashWaitErase(BaseFlash*);
int efl_lld_program(void*,flash_offset_t,size_t,const uint8_t*);
int efl_lld_start_erase_all(void*);
int efl_lld_start_erase_sector(void*,flash_sector_t);
void efl_lld_start(EFlashDriver*);
