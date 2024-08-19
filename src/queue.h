#pragma once

#include <stddef.h>

#include "semaphore.h"

struct queue_node;

/*
 * Queue node. This struct must be at the top of the outer struct which has
 * data associated with this queue node. Otherwise queue_read() won't work.
 */
typedef struct queue_node {
    struct queue_node *p_next;
} queue_node_t;

typedef struct {
    semaphore_t num_nodes;

    queue_node_t *p_head;
    queue_node_t *p_tail;
} queue_t;

void queue_init(queue_t *p_queue);
void queue_write(queue_t *p_queue, queue_node_t *p_node);
void queue_read(queue_t *p_queue, void *p_buf, size_t item_size);
