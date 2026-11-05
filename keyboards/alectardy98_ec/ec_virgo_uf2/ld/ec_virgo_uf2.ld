/* SPDX-License-Identifier: GPL-2.0-or-later
 * TinyUF2: sectors 0-1; settings: sector 2; sector 3 reserved.
 */
f4xx_flash_size = 256k;
f4xx_ram1_size = 128k;
f4xx_ram2_size = 0;
f4xx_ram4_size = 0;
f4xx_ram5_size = 0;
INCLUDE platforms/chibios/boards/common/ld/stm32f4xx_tinyuf2_common.ld
/* Override the generic 64 KiB mailbox to match this board's bootloader. */
_board_dfu_dbl_tap = 0x2001fffc;
__heap_end__ = _board_dfu_dbl_tap;
ASSERT(__flash0_base__ == 0x08010000, "Incorrect TinyUF2 app base")
ASSERT(__flash0_end__ == 0x08040000, "Incorrect flash boundary")
ASSERT(__flash0_free__ <= 0x08040000, "Application overflow")
ASSERT(__ram0_end__ == 0x20020000, "Incorrect SRAM boundary")
ASSERT(__bss_end__ <= _board_dfu_dbl_tap, "RAM overlaps reset mailbox")
