#pragma once

#include <stddef.h>

#include "semaphore.h"

struct queue_item;

typedef struct queue_item {
    struct queue_item *p_next;
} queue_item_t;

typedef struct {
    semaphore_t sem_num_items;

    queue_item_t *p_head;
    queue_item_t *p_tail;
} queue_t;

void queue_init(queue_t *p_queue);
void queue_write(queue_t *p_queue, queue_item_t *p_item);
void queue_read(queue_t *p_queue, void *p_buf, size_t item_size);
