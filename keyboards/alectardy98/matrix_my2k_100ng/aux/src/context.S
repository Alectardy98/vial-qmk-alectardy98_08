/* SPDX-License-Identifier: GPL-2.0-or-later */
.syntax unified
.cpu cortex-m7
.fpu fpv5-sp-d16
.thumb
.section .text.aux_context_switch,"ax",%progbits
.global aux_context_switch
.type aux_context_switch,%function
.thumb_func
aux_context_switch:
 /* 104-byte aligned frame: d8..d15, FPSCR, r4..r11, return PC.
  * AAPCS caller-saved registers live in the suspended C call's frame. Both
  * contexts use MSP; exceptions use the currently active stack and return
  * before this function continues. No PRIMASK/CONTROL/PSP modification. */
 push {r4-r11,lr}
 vmrs r2,fpscr
 push {r2}
 vpush {d8-d15}
 str sp,[r0]
 mov sp,r1
 vpop {d8-d15}
 pop {r2}
 vmsr fpscr,r2
 pop {r4-r11,pc}
.size aux_context_switch,.-aux_context_switch
