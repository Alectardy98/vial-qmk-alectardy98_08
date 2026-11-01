/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef AUX_WORK_H
#define AUX_WORK_H
#include <stdbool.h>
#include <stdint.h>
/* One stackful foreground-only worker. All filesystem/SPI/I2C/SD activity has
 * this single owner. Interrupt handlers never switch contexts. */
void aux_work_init(void (*entry)(void));
void aux_work_resume(void);
void aux_work_yield(void);
/* Yield after a complete idle pass. Unlike a fairness checkpoint this permits
 * foreground WFI. Pending transfers/chunks must use aux_work_yield instead. */
void                     aux_work_idle(void);
void                     aux_work_checkpoint(void);
bool                     aux_work_active(void);
bool                     aux_work_runnable(void);
extern volatile uint32_t aux_work_switches, aux_work_stack_fault;
extern volatile uint32_t aux_work_runnable_yields, aux_work_idle_yields, aux_work_max_slice_ms;
#endif
