/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_USBH_CHIBIOS_H
#define MY2K_USBH_CHIBIOS_H
#include "my2k_scsi.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Actual ChibiOS-Contrib FS-host/MSD adapter. Startup is explicit; caller must
 * establish AUX power/reset sequencing before calling. No unknown power GPIO,
 * format, reset-to-loader or flash operation is implemented. Stock-parity file
 * writes use bounded sectors and realWRITE10/SYNCHRONIZECACHE responses. */
bool my2k_usbh_start(void);
/* Sticky host-only fail-stop reason; zero means no bounded-wait failure. */
uint32_t my2k_usbh_fault(void);
/* Cached exact AUX MSC identity: MY2K-EXP / Asset storage RW / 0005,
 * plus 0006 only for the Dev6 profile. */
bool my2k_usbh_write_profile_ready(void);
/* Cached RAM-only diagnostics; snapshot never invokes USB, SD or file I/O. */
typedef enum {
    MY2K_HOST_OFF=0, MY2K_HOST_IDLE=1, MY2K_HOST_CONNECT=2,
    MY2K_HOST_INFO=3, MY2K_HOST_BLOCK_READ=4, MY2K_HOST_FILE=5,
    MY2K_HOST_CANCEL_SYNC=6, MY2K_HOST_FILE_READ=7,
    MY2K_HOST_FILE_WRITE=8, MY2K_HOST_FILE_SYNC=9
} my2k_host_phase;
typedef struct {
    uint32_t blocks,generation,connect_attempts,connect_returns,connect_failures;
    uint8_t flags,host_state,lun_state,worker_phase,read_state,file_state,last_connect_result,file_command;
} my2k_usbh_diagnostics;
void my2k_usbh_get_diagnostics(my2k_usbh_diagnostics *out);
/* Mode transition admission: no queued I/O or open GUI write handle. */
bool my2k_usbh_asset_idle(void);
typedef struct {
    uint8_t inquiry[36];
    uint16_t vendor_id,product_id;
    bool device_valid,inquiry_valid;
} my2k_usbh_identity;
void my2k_usbh_get_identity(my2k_usbh_identity *out);
#if MY2K_DEV15_PROFILE
bool my2k_usbh_performance_page(uint8_t index,uint8_t report[32]);
#endif


/* Consume a real internal-MSC disconnect event on the QMK thread. */
bool my2k_usbh_take_disconnect(void);
bool my2k_usbh_capacity(uint32_t *blocks, uint16_t *size);
/* One bounded asynchronous read in flight; worker performs blocking USB I/O,
 * leaving QMK matrix and PC-device service outside the blocking path. */
bool my2k_usbh_submit_read(uint32_t lba, uint16_t offset, uint16_t bytes);
bool my2k_usbh_take_read(void *dst, size_t capacity, int32_t *result);
/* Deferred read-only file requests share the worker, never the matrix/ISR.
 * Duplicate in-flight requests coalesce; a different request cancels delivery
 * of the old response rather than emitting a stale HID packet later. */
bool my2k_usbh_file_request(const uint8_t report[32]);
bool my2k_usbh_file_take_reply(uint8_t report[32]);
void my2k_usbh_file_cancel_reply(void);
/* Explicit asynchronous SCSI-service binding; exclusive queue ownership. */
void my2k_usbh_scsi_init(my2k_scsi *service);
#endif

#if MY2K_RELEASE1_PROFILE
void my2k_usbh_files_changed(void);
#endif

bool my2k_usbh_animation_busy(void);

/* Exclusive AFB1 queue. Buffer remains borrowed until finish reports true. */
bool my2k_usbh_fast_request(uint8_t *buffer,uint16_t size);
bool my2k_usbh_fast_done(uint16_t *size);
bool my2k_usbh_fast_finish(bool cancel);

/* IRQ-class wake only; caller holds the system lock. No filesystem work. */
void my2k_usbh_fast_wakeI(void);
