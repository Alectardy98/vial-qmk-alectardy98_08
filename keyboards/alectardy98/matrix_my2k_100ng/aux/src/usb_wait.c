/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_msc.h"
#include "aux_platform.h"
static volatile bool failed;
volatile uint32_t aux_usb_fault_register;
bool aux_usb_failed(void){return failed;}
void aux_usb_abort(uint32_t register_address){
 /* Callable from a DCD interrupt or foreground critical section. No USB stack
  * calls, worker mutation, waits, or retry occur here. Reset clears the latch. */
 extern void aux_usb_platform_abort(void);
 if(!failed)aux_usb_fault_register=register_address;
 failed=true;aux_fault_code=9;aux_usb_platform_abort();
}
/* Independent board timeout policy, not a claim of a measured PHY limit.
 * Real SysTick bounds normal failures; poll budget also bounds a stopped tick. */
bool aux_usb_wait_bits(volatile uint32_t *reg,uint32_t mask,uint32_t value){
 uint32_t start=aux_millis();
 for(unsigned polls=0;polls<200000u;polls++){
  if((*reg&mask)==value)return true;
  if((uint32_t)(aux_millis()-start)>=20u)return false;
 }
 return false;
}
bool aux_usb_runtime_wait_bits(volatile uint32_t *reg,uint32_t mask,uint32_t value){
 if(aux_usb_failed())return false;
 if(aux_usb_wait_bits(reg,mask,value))return true;
 aux_usb_abort((uint32_t)(uintptr_t)reg);return false;
}
