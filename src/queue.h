#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "semaphore.h"

struct queue_node;

typedef struct queue_node {
    void *p_data;
    struct queue_node *p_next;
} queue_node_t;

typedef struct {
    semaphore_t num_nodes;

    queue_node_t *p_head;
    queue_node_t *p_tail;

    size_t max_items;
    size_t item_size;

    queue_node_t *p_node_storage;
    void *p_item_storage;
    uint32_t *p_storage_usage_map;
} queue_t;

void queue_init(queue_t *p_queue, size_t max_items, size_t item_size);
bool queue_write(queue_t *p_queue, void *p_data);
void queue_read(queue_t *p_queue, void *p_buf, size_t item_size);
