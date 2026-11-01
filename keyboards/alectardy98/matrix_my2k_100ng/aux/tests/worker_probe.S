/* SPDX-License-Identifier: GPL-2.0-or-later */
.syntax unified
.cpu cortex-m7
.fpu fpv5-sp-d16
.thumb
.section .text,"ax",%progbits
.global probe_worker
.type probe_worker,%function
.thumb_func
probe_worker:
 push {r4-r11,lr}
 sub sp,sp,#4
 ldr r4,=0x12345678
 ldr r5,=0x9abcdef0
 vmov d8,r4,r5
 vmov d15,r5,r4
 bl aux_work_yield
 ldr r0,=0x20030000
 stmia r0!,{r4,r5}
 vmov r2,r3,d8
 stmia r0!,{r2,r3}
 vmov r2,r3,d15
 stmia r0!,{r2,r3}
 mov r2,sp
 str r2,[r0]
 add sp,sp,#4
 pop {r4-r11,pc}
.global probe_read
.type probe_read,%function
.thumb_func
probe_read:
 push {r4,lr}
 movs r0,#2
 ldr r1,=0x20030200
 bl aux_sd_read
 ldr r1,=0x20030100
 str r0,[r1]
 pop {r4,pc}
.global probe_init
.type probe_init,%function
.thumb_func
probe_init:
 push {r4,lr}
 bl aux_sd_init
 ldr r1,=0x20030100
 str r0,[r1]
 pop {r4,pc}
.global probe_touch
.type probe_touch,%function
.thumb_func
probe_touch:
 push {r4,lr}
 ldr r0,=0x20030200
 bl aux_touch_read
 ldr r1,=0x20030100
 str r0,[r1]
 pop {r4,pc}
.global probe_delay
.type probe_delay,%function
.thumb_func
probe_delay:
 push {r4,lr}
 movs r0,#5
 bl aux_delay
 ldr r1,=0x20030100
 str r0,[r1]
 pop {r4,pc}
.global probe_spi
.type probe_spi,%function
.thumb_func
probe_spi:
 push {r4,lr}
 bl aux_panel_physical_bus
 ldr r4,[r0,#12]
 ldr r0,[r0]
 movs r1,#0
 ldr r2,=0x20030200
 movs r3,#1
 blx r4
 ldr r1,=0x20030100
 str r0,[r1]
 pop {r4,pc}
.global probe_spi_frame
.type probe_spi_frame,%function
.thumb_func
probe_spi_frame:
 push {r4,lr}
 bl aux_panel_physical_bus
 ldr r4,[r0,#12]
 ldr r0,[r0]
 movs r1,#0
 ldr r2,=0x20020000
 ldr r3,=34176
 blx r4
 ldr r1,=0x20030100
 str r0,[r1]
 pop {r4,pc}
#ifdef AUX_DEV4_PROBE
.global probe_idle
.type probe_idle,%function
.thumb_func
probe_idle:
 push {r4,lr}
 bl aux_work_idle
 movs r0,#1
 ldr r1,=0x20030100
 str r0,[r1]
 pop {r4,pc}
#endif
