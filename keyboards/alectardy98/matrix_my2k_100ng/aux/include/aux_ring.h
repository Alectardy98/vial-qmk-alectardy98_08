/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef AUX_RING_H
#define AUX_RING_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define AUX_RING_CAPACITY 512u
/* Single producer and single consumer on one core (foreground/ISR).
 * One reserved slot; frame bytes are published together after the complete copy.
 * Not a multi-producer queue or a general multi-core synchronization primitive. */
typedef struct {volatile uint8_t bytes[AUX_RING_CAPACITY];volatile uint16_t head,tail;} aux_ring;
void aux_ring_init(aux_ring *queue);
size_t aux_ring_count(const aux_ring *queue);
bool aux_ring_push(aux_ring *queue,const uint8_t *bytes,size_t size);
bool aux_ring_pop(aux_ring *queue,uint8_t *byte);
#endif
