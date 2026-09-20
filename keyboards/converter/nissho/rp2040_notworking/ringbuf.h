#pragma once

#include <stdint.h>
#include <stdbool.h>

#define RBUF_SIZE 32

typedef struct {
    uint8_t buffer[RBUF_SIZE];
    volatile uint8_t head;
    volatile uint8_t tail;
    volatile uint8_t count;
} ringbuf_t;

static inline void ringbuf_init(ringbuf_t *rb) {
    rb->head = rb->tail = rb->count = 0;
}

static inline bool ringbuf_is_empty(const ringbuf_t *rb) {
    return rb->count == 0;
}

static inline bool ringbuf_is_full(const ringbuf_t *rb) {
    return rb->count >= RBUF_SIZE;
}

static inline bool ringbuf_enqueue(ringbuf_t *rb, uint8_t data) {
    if (ringbuf_is_full(rb)) return false;
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) % RBUF_SIZE;
    rb->count++;
    return true;
}

static inline uint8_t ringbuf_dequeue(ringbuf_t *rb) {
    if (ringbuf_is_empty(rb)) return 0;
    uint8_t data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RBUF_SIZE;
    rb->count--;
    return data;
}
