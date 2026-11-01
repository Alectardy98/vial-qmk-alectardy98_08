/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef AUX_STORAGE_H
#define AUX_STORAGE_H
#include <stdint.h>
enum {
    AUX_STORAGE_WAITING,
    AUX_STORAGE_SD_FAILED,
    AUX_STORAGE_MOUNT_FAILED,
    AUX_STORAGE_READY,
    AUX_STORAGE_NO_ASSETS,
    AUX_STORAGE_DEFERRED
};
typedef struct {
    uint32_t attempts, sd_attempts, mount_attempts, deferred, sectors, next_retry;
    uint8_t  stage;
} aux_storage_diagnostics;
/* Cached RAM only; no command or I/O is performed by reading this structure. */
extern volatile aux_storage_diagnostics aux_storage_live;
/* SD driver's last completed/pending operation. RAM observation only. */
extern volatile uint32_t aux_sd_init_stage, aux_sd_init_attempts;
extern volatile uint32_t aux_sd_command_count, aux_sd_last_command, aux_sd_last_status, aux_sd_last_response;
extern volatile uint32_t aux_sd_read_attempts, aux_sd_read_successes, aux_sd_read_failures, aux_sd_read_ready_queries;
extern volatile uint32_t aux_sd_last_read_lba, aux_sd_last_read_status, aux_sd_last_read_words, aux_sd_read_fail_stage;
/* Worker-only: retries existing initialization, backoff1/2/4/8/16/30seconds and
 * never while MSC owns a request/write lease or an asset volume is mounted. */
void aux_storage_start(void);
void aux_storage_service(uint32_t now);
#endif
