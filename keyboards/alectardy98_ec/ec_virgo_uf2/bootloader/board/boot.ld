/* SPDX-License-Identifier: MIT
 * EC Virgo bootloader memory contract, independently specified.
 * Reserve top of 128 KiB RAM for the QMK/TinyUF2 mailbox.
 */
ENTRY(Reset_Handler)
MEMORY {
 FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 31K
 CONFIG (rx) : ORIGIN = 0x08007C00, LENGTH = 1K
 RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 128K - 4
}
_estack = (ORIGIN(RAM) + LENGTH(RAM)) & ~7;
_board_dfu_dbl_tap = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS {
 .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
 .text : { *(.text*) *(.rodata*) *(.glue_7*) KEEP(*(.init)) KEEP(*(.fini)) } > FLASH
 .ARM.extab : { *(.ARM.extab*) } > FLASH
 .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
 .preinit_array : { __preinit_array_start = .; KEEP(*(.preinit_array*)) __preinit_array_end = .; } > FLASH
 .init_array : { __init_array_start = .; KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array)) __init_array_end = .; } > FLASH
 .fini_array : { __fini_array_start = .; KEEP(*(.fini_array*)) __fini_array_end = .; } > FLASH
 .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT> FLASH
 _sidata = LOADADDR(.data);
 .bss (NOLOAD) : { . = ALIGN(4); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM
 .heap_stack (NOLOAD) : { . = ALIGN(8); end = .; _end = .; . += 0x600; } > RAM
 .config : { KEEP(*(.config)) } > CONFIG
 .ARM.attributes 0 : { *(.ARM.attributes) }
}
ASSERT(LOADADDR(.data) + SIZEOF(.data) <= 0x08007C00, "Bootloader overflow")

ASSERT(_board_dfu_dbl_tap == 0x2001FFFC, "QMK mailbox mismatch")
ASSERT(_estack == 0x2001FFF8, "Bootloader stack alignment")
