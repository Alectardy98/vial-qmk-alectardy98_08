/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Foreground only copies packets. All FatFs work stays in the unique worker. */
bool aux_animation_files_submit(const uint8_t packet[32]);
void aux_animation_files_reply(uint8_t packet[32]);
void aux_animation_files_service(void);
bool aux_animation_files_busy(void);
/* True only inside the unique worker's bounded filesystem mutation scope. */
bool aux_animation_files_writing(void);
bool aux_animation_files_export_ready(void);
bool aux_animation_slot_path(unsigned slot, char path[64]);

/* AFB1 shares the MSC completion buffer under an exclusive control lease. */
uint8_t *aux_animation_fast_receive(uint16_t size);
bool aux_animation_fast_submit(void);
uint8_t *aux_animation_fast_reply(uint16_t *size);
void aux_animation_fast_ack(void);
void aux_animation_fast_cancel(void);

/* AFB2 pipelined, AUX-owned vendor bulk writes. Completion is explicit. */
bool aux_animation_bulk_submit(const uint8_t cdb[16],const void *data,uint16_t size);
void aux_animation_bulk_status(uint8_t out[32]);
bool aux_animation_files_pending(void);

int32_t aux_animation_bulk_read(const uint8_t cdb[16],void *buffer,uint16_t size);

/* Called only by the exclusive I/O worker, never from an IRQ. */
int32_t aux_animation_direct_read(const uint8_t cdb[16],void *buffer,uint16_t size);
