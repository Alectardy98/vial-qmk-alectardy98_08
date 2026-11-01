/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Keyboard execution only while Main is inside its synchronous RGB DMA call.
 * Main joins a yielded QMK action before returning from that call. No second
 * RGB owner, timer/ISR scanning, GPIO change or shared HAL modification. */
#include "ch.h"
#include "hal.h"
#include "chibios_config.h"
#include "main_input_io.h"
#include "main_input_scheduler.h"
#include "main_perf.h"
#include <string.h>
static THD_WORKING_AREA(input_stack,2048);
static binary_semaphore_t start,finished;
static thread_t *worker;
static bool initialized,open,working;
static uint32_t windows,callbacks,joins,refused,max_window_us,opened_at;
_Static_assert(CH_CFG_ST_FREQUENCY>=8000,"Input clock cannot resolve8k average");
static THD_FUNCTION(input_worker,arg){
 (void)arg;chRegSetThreadName("my2k-input-io");
 for(;;){
  chBSemWait(&start);
  for(;;){
   chSysLock();bool go=open;chSysUnlock();if(!go)break;
   /* Sleep most of the remaining internal scan period. One native tick is the
    * maximum bounded spin: avoids tick-rounding overhead pushing a due scan into the next USB microframe. */
   unsigned left=my2k_input_scheduler_wait_us();
   const unsigned tick_us=(1000000u+CH_CFG_ST_FREQUENCY-1u)/CH_CFG_ST_FREQUENCY;
   if(left>tick_us){chThdSleepMicroseconds(left-tick_us);continue;}
   unsigned polls=512;
   while(left&&polls--)left=my2k_input_scheduler_wait_us();
   if(left){chThdSleepMicroseconds(tick_us);continue;} /* stopped clock: yield */
   chSysLock();go=open;if(go)working=true;chSysUnlock();if(!go)break;
   bool ran=my2k_input_scheduler_checkpoint(true);
   chSysLock();if(ran)callbacks++;working=false;go=open;if(!go)chBSemSignalI(&finished);chSysUnlock();if(!go)break;
  }
 }
}
void my2k_input_io_init(void){
 if(initialized)return;
 initialized=true;chBSemObjectInit(&start,true);chBSemObjectInit(&finished,true);
 worker=chThdCreateStatic(input_stack,sizeof input_stack,NORMALPRIO+1,input_worker,NULL);
}
bool my2k_input_io_begin(void){
 if(!worker||chThdGetSelfX()!=&(currcore->mainthread)||!my2k_input_scheduler_can_io())return false;
 chSysLock();if(open){refused++;chSysUnlock();return false;}
 open=true;windows++;opened_at=my2k_perf_clock();chSysUnlock();chBSemSignal(&start);return true;
}
void my2k_input_io_end(void){
 chSysLock();open=false;
 /* A user macro can yield inside keyboard_task. Never allow Main's next
  * keyboard_task or touch action to overlap it, even after DMA completion. */
 while(working){joins++;(void)chBSemWaitS(&finished);}
 uint32_t us=(my2k_perf_clock()-opened_at)/(REALTIME_COUNTER_CLOCK/1000000u);
 if(us>max_window_us)max_window_us=us;
 chSysUnlock();
}
bool my2k_input_io_status(uint8_t r[32]){
 chSysLock();memset(r+4,0,28);r[4]=1;r[5]=2;r[7]=100;r[6]=(worker?1u:0u)|(open?2u:0u)|(working?4u:0u);
 const uint32_t data[]={windows,callbacks,joins,refused,max_window_us,sizeof input_stack};
 for(unsigned i=0;i<6;i++)for(unsigned j=0;j<4;j++)r[8+4*i+j]=(uint8_t)(data[i]>>(8*j));
 chSysUnlock();return true;
}
