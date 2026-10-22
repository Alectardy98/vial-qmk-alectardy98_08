// SPDX-License-Identifier: GPL-2.0-or-later
/* Policy only: QMK owns EEPROM format/replay and ChibiOS owns flash operations. */
#include <hal.h>
#include "diag.h"
void actuator_storage_quiet(void);
#include "wear_leveling.h"
#include "wear_leveling_internal.h"

bool s2_armed;
bool s2_qmk_init(void);
bool s2_qmk_read(uint32_t address, uint32_t *value);
bool s2_qmk_unlock(void);
void s2_hw_start(EFlashDriver *p) __attribute__((noinline,noclone));
flash_error_t s2_hw_program(void *,flash_offset_t,size_t,const uint8_t *) __attribute__((noinline,noclone));
flash_error_t s2_hw_erase_sector(void *,flash_sector_t) __attribute__((noinline,noclone));

_Static_assert(WEAR_LEVELING_LOGICAL_SIZE==4096U,"Logical EEPROM changed");
_Static_assert(WEAR_LEVELING_BACKING_SIZE==0x4000U,"Wrong backing allocation");
_Static_assert(WEAR_LEVELING_EFL_FIRST_SECTOR==2,"Wrong storage sector");
_Static_assert(WEAR_LEVELING_EFL_OMIT_LAST_SECTOR_COUNT==2,"Unsafe sector search");
_Static_assert(WEAR_LEVELING_EFL_FLASH_SIZE==0x20000U,"Wrong density");
_Static_assert(BACKING_STORE_WRITE_SIZE==4,"Unexpected write granularity");

uint16_t s2_factory_kib(void) {
#ifdef DIAG_HOST_TEST
 extern uint16_t host_factory_kib;
 return host_factory_kib;
#else
 return *(volatile const uint16_t *)(uintptr_t)0x1FFF7A22U;
#endif
}
uint32_t s2_option_bits(void) {
#ifdef DIAG_HOST_TEST
 extern uint32_t host_optcr;
 uint32_t value=host_optcr;
#else
 uint32_t value=FLASH->OPTCR; /* Volatile, read-only. Never unlock option bytes. */
#endif
 efl_diag_record.option_bits=value;
 return value;
}
static bool geometry(void) {
 const uint32_t opt=s2_option_bits();
 /* Normal WRP mode, preserve protected sectors 0/1, require writable sector 2. */
 if ((opt & 0x80000000U) || (opt & 0x00030000U) || !(opt & 0x00040000U)) {
  s2_armed=false;efl_diag_record.error=11;return false;
 }

 if (s2_factory_kib()!=128 || !diag_inspect_geometry()) {
  s2_armed=false;
  if (!efl_diag_record.error) efl_diag_record.error=8;
  return false;
 }
 return true;
}
static flash_error_t reject(void) {
 s2_armed=false;
 efl_diag_record.blocked_ops++;
 if (!efl_diag_record.error) efl_diag_record.error=9;
 return FLASH_ERROR_UNIMPLEMENTED;
}
static bool ready(void *p) {
 return p==&EFLD1 && s2_armed && efl_diag_record.backend_ok && geometry();
}
bool backing_store_init(void) {
 s2_armed=false;efl_diag_record.backend_ok=0;
 diag_mark(CP_BACKEND_ENTER);
 if (!geometry()) return false;
 diag_mark(CP_PHYSICAL_ENTER);
 if (!s2_qmk_init()) { efl_diag_record.error=4;return false; }
 diag_mark(CP_PHYSICAL_RETURN);
 efl_diag_record.backend_offset=0x8000U;efl_diag_record.backend_sector_count=1;
 efl_diag_record.backend_ok=1;
 return true;
}
bool backing_store_read(uint32_t address,backing_store_int_t *value) {
 if (!value || address%4 || address>0x3FFCU || !efl_diag_record.backend_ok || !geometry()) return false;
 diag_mark(CP_READING);efl_diag_record.physical_reads++;efl_diag_record.last_read=address;
 return s2_qmk_read(address,value);
}
bool backing_store_unlock(void) {
 actuator_storage_quiet();
 if (!ready(&EFLD1) || (EFLD1.state!=FLASH_STOP && EFLD1.state!=FLASH_READY)) { reject();return false; }
 return s2_qmk_unlock() && s2_armed;
}
void s2_storage_init(void) {
 s2_armed=false;diag_mark(CP_WL_ENTER);
 efl_diag_record.wl_status=wear_leveling_init();
 if (efl_diag_record.wl_status==WEAR_LEVELING_FAILED) {
  s2_armed=false;
  if (!efl_diag_record.error) efl_diag_record.error=6;
  /* No destructive boot repair. Relock only a validated controller handle. */
  if (EFLD1.flash==FLASH && (EFLD1.state==FLASH_STOP || EFLD1.state==FLASH_READY)) eflStop(&EFLD1);
 } else if (efl_diag_record.backend_ok && geometry()) {
  s2_armed=true;
 }
 diag_mark(CP_WL_RETURN);
}
/* The standard EFL VMT resolves exclusively to these wrappers. */
__attribute__((noinline,noclone)) void efl_lld_start(EFlashDriver *p) {
 actuator_storage_quiet();
 if (!ready(p) || (EFLD1.state!=FLASH_STOP && EFLD1.state!=FLASH_READY)) { reject();return; }
 s2_hw_start(p);efl_diag_record.unlock_ops++;
}
__attribute__((noinline,noclone)) flash_error_t efl_lld_program(void *p,flash_offset_t offset,size_t n,const uint8_t *data) {
 actuator_storage_quiet();
 if (!ready(p) || EFLD1.state!=FLASH_READY || !data || offset<0x8000U || offset>=0xC000U ||
     offset%4U || !n || n%4U || n>0xC000U-offset) return reject();
 efl_diag_record.program_ops++;
 flash_error_t result=s2_hw_program(p,offset,n,data);
 efl_diag_record.last_flash_status=(uint32_t)result;
 if (result!=FLASH_NO_ERROR) { s2_armed=false;efl_diag_record.flash_errors++;efl_diag_record.error=10; }
 return result;
}
__attribute__((noinline,noclone)) flash_error_t efl_lld_start_erase_sector(void *p,flash_sector_t sector) {
 actuator_storage_quiet();
 if (!ready(p) || EFLD1.state!=FLASH_READY || sector!=2) return reject();
 efl_diag_record.erase_ops++;
 flash_error_t result=s2_hw_erase_sector(p,sector);
 efl_diag_record.last_flash_status=(uint32_t)result;
 if (result!=FLASH_NO_ERROR) { s2_armed=false;efl_diag_record.flash_errors++;efl_diag_record.error=10; }
 return result;
}
__attribute__((noinline,noclone)) flash_error_t efl_lld_start_erase_all(void *p) {
 (void)p;return reject();
}

/* Complete/poll standard erases even if a fault has disabled further starts. */
flash_error_t s2_hw_query_erase(void *,uint32_t *) __attribute__((noinline,noclone));
__attribute__((noinline,noclone)) flash_error_t efl_lld_query_erase(void *p,uint32_t *msec) {
 if (p!=&EFLD1 || EFLD1.flash!=FLASH) return reject();
 flash_error_t result=s2_hw_query_erase(p,msec);
 if (result!=FLASH_NO_ERROR && result!=FLASH_BUSY_ERASING) efl_diag_record.last_flash_status=(uint32_t)result;
 if (result!=FLASH_NO_ERROR && result!=FLASH_BUSY_ERASING) {
  s2_armed=false;efl_diag_record.flash_errors++;efl_diag_record.error=10;
 }
 return result;
}
