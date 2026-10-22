// SPDX-License-Identifier: GPL-2.0-or-later
#include <hal.h>
#include "diag.h"
#ifndef DIAG_HOST_TEST
extern const uint8_t __flash0_free__[];
#define DIAG_TABLE_START ((uintptr_t)0x08010000U)
#define DIAG_TABLE_END ((uintptr_t)__flash0_free__)
#endif
static bool table_range(const void *p, size_t bytes) {
 uintptr_t a=(uintptr_t)p;
 return a%4U==0 && a>=DIAG_TABLE_START && a<=DIAG_TABLE_END && bytes<=DIAG_TABLE_END-a;
}
bool diag_inspect_geometry(void) {
 const flash_descriptor_t *d=EFLD1.descriptor;
 efl_diag_record.descriptor=(uint32_t)(uintptr_t)d;
 if (!table_range(d,sizeof(*d))) { efl_diag_record.error=1; return false; }
 efl_diag_record.flash_size=d->size; efl_diag_record.sectors=d->sectors_count;
 if (EFLD1.flash!=FLASH || d->address!=(uint8_t *)0x08000000U ||
     d->size!=0x20000U || d->sectors_count!=5 || d->sectors_size!=0 ||
     d->page_size!=4 || !(d->attributes&FLASH_ATTR_ERASED_IS_ONE) ||
     !(d->attributes&FLASH_ATTR_MEMORY_MAPPED) ||
     !table_range(d->sectors,5*sizeof(*d->sectors))) { efl_diag_record.error=2; return false; }
 // Exact five-sector F401 geometry; no descriptor field or bound is skipped.
 for (unsigned i=0;i<5;i++) if (d->sectors[i].offset!=(i*0x4000U) || d->sectors[i].size!=(i==4?0x10000U:0x4000U)) {
   efl_diag_record.error=3; return false;
 }
 efl_diag_record.sector2_offset=d->sectors[2].offset;
 efl_diag_record.sector2_size=d->sectors[2].size;
 diag_mark(CP_GEOMETRY_OK); return true;
}
