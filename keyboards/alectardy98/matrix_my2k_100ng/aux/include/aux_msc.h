/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef AUX_MSC_H
#define AUX_MSC_H
#include <stdbool.h>
#include <stdint.h>
/* Pure backend separately tested without USB/controller access. */
int32_t aux_msc_read(uint8_t lun,uint32_t lba,uint32_t offset,void *buffer,uint32_t size);
/* Queue in USB foreground, service only in the unique I/O worker. */
int32_t aux_msc_queue(uint8_t lun,uint32_t lba,uint32_t offset,void *buffer,uint32_t size);
void aux_msc_worker_service(void);
bool aux_msc_take_completion(int32_t *bytes);
void aux_msc_cancel(void);
uint32_t aux_msc_generation(void);
#if MY2K_STOCK_PARITY_PROFILE
int32_t aux_msc_write(uint8_t lun,uint32_t lba,uint32_t offset,const void *buffer,uint32_t size);
int32_t aux_msc_queue_write(uint8_t lun,uint32_t lba,uint32_t offset,const void *buffer,uint32_t size);
uint32_t aux_msc_media_generation(void);
bool aux_msc_media_writing(void);
bool aux_msc_media_sync(void);
/* True only when no queued/running/completed request owns the SD transport. */
bool aux_msc_io_idle(void);
#endif
bool aux_usb_wait_bits(volatile uint32_t *reg,uint32_t mask,uint32_t value);
/* Sticky DCD failure: IRQ masks/disconnect immediately; MSC cancellation is
 * deferred to foreground so an interrupted worker cannot corrupt its epoch. */
bool aux_usb_failed(void);
void aux_usb_abort(uint32_t register_address);
bool aux_usb_runtime_wait_bits(volatile uint32_t *reg,uint32_t mask,uint32_t value);
extern volatile uint32_t aux_usb_fault_register;
bool aux_usb_init(void);
void aux_usb_task(void);
#endif

#if MY2K_RELEASE1_PROFILE
bool aux_msc_export_active(void);
void aux_msc_set_export(bool);
#endif

#if MY2K_ANIMATION_PROFILE
uint8_t *aux_msc_control_acquire(void);
void aux_msc_control_release(void);
#endif

uint8_t *aux_msc_control_second(void);

#if MY2K_DOOM_PROFILE
/* Exclusive game cache loan. Normal control/MSC requests remain blocked until
 * release; no buffer is shared with an in-flight filesystem or USB transfer. */
bool aux_msc_game_buffers(uint8_t **first,uint8_t **second);
void aux_msc_game_release(void);
#endif

/* NegotiatedC3 slot reads share the epoch-guarded USB I/O worker. */
int32_t aux_msc_queue_file(const uint8_t command[16],void *buffer,uint16_t size);
