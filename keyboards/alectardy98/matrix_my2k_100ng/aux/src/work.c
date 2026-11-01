/* SPDX-License-Identifier: GPL-2.0-or-later */
#if MY2K_DEV22_PROFILE
#include "aux_perf22.h"
#endif
#include "aux_work.h"
#include "aux_platform.h"
#include <stddef.h>
#define WORK_STACK_WORDS 4096u
#define CANARY 0x41555857u
static uint32_t worker_stack[WORK_STACK_WORDS] __attribute__((aligned(8)));
static uint32_t *worker_sp,*foreground_sp;
static void (*worker_entry)(void);
static bool running,initialized,finished,runnable;
static uint32_t slice_start,polls;
volatile uint32_t aux_work_switches,aux_work_stack_fault;
volatile uint32_t aux_work_runnable_yields,aux_work_idle_yields,aux_work_max_slice_ms;
extern void aux_context_switch(uint32_t **save,uint32_t *restore);
static void worker_start(void){
 worker_entry();finished=true;for(;;)aux_work_yield();
}
void aux_work_init(void (*entry)(void)){
 running=false;finished=false;initialized=entry!=NULL;runnable=initialized;worker_entry=entry;
 for(unsigned i=0;i<WORK_STACK_WORDS;i++)worker_stack[i]=CANARY;
 worker_sp=worker_stack+WORK_STACK_WORDS-26u;
 for(unsigned i=0;i<26;i++)worker_sp[i]=0;
 worker_sp[25]=(uint32_t)(uintptr_t)worker_start|1u;
 aux_work_stack_fault=0;aux_work_switches=0;
 aux_work_runnable_yields=aux_work_idle_yields=aux_work_max_slice_ms=0;
}
bool aux_work_active(void){return initialized&&!finished&&!aux_work_stack_fault;}
bool aux_work_runnable(void){return aux_work_active()&&runnable;}
void aux_work_resume(void){
 if(!aux_work_active()||running)return;
 for(unsigned i=0;i<16;i++)if(worker_stack[i]!=CANARY){aux_work_stack_fault=1;aux_fault_code=7;return;}
 running=true;runnable=true;polls=0;slice_start=aux_millis_fast();aux_work_switches++;
 aux_context_switch(&foreground_sp,worker_sp);running=false;
}
void aux_work_yield(void){
 if(!running)return;
 runnable=true;aux_work_runnable_yields++;
#if MY2K_DEV22_PROFILE
 aux_perf22_count(PERF22_YIELD_CALL);bool measuring=aux_perf22_enter(PERF22_SUSPENDED);
#endif
 running=false;aux_context_switch(&worker_sp,foreground_sp);running=true;
#if MY2K_DEV22_PROFILE
 aux_perf22_leave(measuring);
#endif
}
void aux_work_idle(void){
 if(!running)return;
 runnable=false;aux_work_idle_yields++;
#if MY2K_DEV22_PROFILE
 aux_perf22_count(PERF22_YIELD_CALL);bool measuring=aux_perf22_enter(PERF22_SUSPENDED);
#endif
 running=false;aux_context_switch(&worker_sp,foreground_sp);running=true;
#if MY2K_DEV22_PROFILE
 aux_perf22_leave(measuring);
#endif
}
void aux_work_checkpoint(void){
 if(!running)return;
 uint32_t elapsed=(uint32_t)(aux_millis_fast()-slice_start);
 if(elapsed>=1u||++polls>=128u){
  if(elapsed>aux_work_max_slice_ms)aux_work_max_slice_ms=elapsed;
  aux_work_yield();
 }
}
