/* Dedicated game profile; fully backed up AUX, SWD install only. */
/* SPDX-License-Identifier: GPL-2.0-or-later */
ENTRY(Reset_Handler)
MEMORY {
 /* Stock AUX UF2 occupies sectors 0..4 (last payload ends at 0801BD00).
  * Restrict this application to that complete 128KiB erase footprint. Neither
  * the dump's FF tail nor its filename establishes ownership above 08020000. */
 FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 512K
 ITCM (rwx) : ORIGIN = 0x00000100, LENGTH = 16128
 RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}
__stack_top = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS {
 .isr_vector : ALIGN(512) { KEEP(*(.isr_vector)) } > FLASH
 .gbc_itcm : { . = ALIGN(4); __gbc_itcm_start = .; *(.gbc_itcm*) . = ALIGN(64); __gbc_itcm_end = .; } > ITCM AT> FLASH
 __gbc_itcm_load = LOADADDR(.gbc_itcm);
 .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
 .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
 .data : { . = ALIGN(4); __data_start = .; *(.data*) . = ALIGN(4); __data_end = .; } > RAM AT> FLASH
 __data_load = LOADADDR(.data);
 .doom_data : { . = ALIGN(8); __doom_data_start = .; *(.doom_data*) . = ALIGN(8); __doom_data_end = .; } > RAM AT> FLASH
 __doom_data_load = LOADADDR(.doom_data) + __doom_data_start - ADDR(.doom_data);
 .nes_bss (NOLOAD) : { . = ALIGN(8); __nes_bss_start = .; *(.nes_bss*) . = ALIGN(8); __nes_bss_end = .; } > RAM
 .doom_bss ADDR(.nes_bss) (NOLOAD) : { . = ALIGN(8); __doom_bss_start = .; *(.doom_bss*) . = ALIGN(8); __doom_bss_end = .; } > RAM
 .gbc_bss ADDR(.nes_bss) (NOLOAD) : { . = ALIGN(8); __gbc_bss_start = .; __gbc_core_start = .; *(.gbc_bss.core) __gbc_core_end = .; *(.gbc_bss*) . = ALIGN(8); __gbc_bss_end = .; } > RAM
 .bss MAX(MAX(__nes_bss_end, __doom_bss_end), __gbc_bss_end) (NOLOAD) : { . = ALIGN(8); __bss_start = .; *(.bss*) *(COMMON) . = ALIGN(8); __bss_end = .; } > RAM
 end = __bss_end;
 /DISCARD/ : { *(.comment*) *(.note*) }
 ASSERT(ADDR(.isr_vector) == 0x08000000, "ASTRO AUX vectors must start at08000000")
 ASSERT(SIZEOF(.isr_vector) == 512, "AUX requires its complete128-entry vector table")
 ASSERT(Reset_Handler >= ADDR(.text) && Reset_Handler < ADDR(.text)+SIZEOF(.text), "AUX reset must be application code")
 ASSERT((__stack_top & 7) == 0 && __stack_top == 0x20040000, "AUX stack must match recovered SRAM top")
 ASSERT((__data_start & 3) == 0 && (__data_end & 3) == 0 && (__bss_start & 3) == 0 && (__bss_end & 3) == 0, "AUX startup copies whole words")
 ASSERT(__bss_end + 8192 <= __stack_top, "AUX RAM needs8KiB stack margin")
 ASSERT(LOADADDR(.doom_data)+SIZEOF(.doom_data) <= ORIGIN(FLASH)+LENGTH(FLASH), "AUX flash overflow")
}

ASSERT(__gbc_core_end <= 0x20010000 && __gbc_core_end-__gbc_core_start > 49000, "GBC core must fit DTCM")
ASSERT(SIZEOF(.gbc_itcm) <= 16128, "GBC ITCM overflow")
ASSERT(__gbc_itcm_start == 0x100 && LENGTH(ITCM) >= 12288, "Doom CPU-only loan fits GBC ITCM")
