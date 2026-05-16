/*
 * Ring buffer API.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *p_start;
    uint8_t *p_read;
    uint8_t *p_write;

    size_t size;
    size_t bytes_used;
} ringbuf_t;

void ringbuf_init(ringbuf_t *p_ringbuf, void *p_start, size_t size);
void ringbuf_lock(ringbuf_t *p_ringbuf);
void ringbuf_unlock(ringbuf_t *p_ringbuf);

size_t ringbuf_read(ringbuf_t *p_ringbuf, void *p_buf, size_t num_bytes);
size_t ringbuf_write(ringbuf_t *p_ringbuf, void const *p_buf, size_t num_bytes);
void ringbuf_update_write_ptr(ringbuf_t *p_ringbuf, size_t offset_from_start);

size_t ringbuf_bytes_used(ringbuf_t *p_ringbuf);
bool ringbuf_is_empty(ringbuf_t *p_ringbuf);
