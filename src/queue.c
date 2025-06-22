/*
 * Bounded multi-producer multi-consumer FIFO queue.
 */

#include <stdatomic.h>
#include <stdbool.h>

#include "assert.h"
#include "heap.h"
#include "panic.h"
#include "queue.h"

static queue_node_t *new_node(queue_t *p_queue, void *p_data);
static void free_node(queue_t *p_queue, queue_node_t *p_node);

void queue_init(queue_t *p_queue, size_t max_items, size_t item_size) {
    ASSERT(max_items % 32 == 0);

    queue_node_t *p_node_storage = heap_alloc(max_items * sizeof(queue_node_t));
    void *p_item_storage = heap_alloc(max_items * item_size);
    uint32_t *p_storage_usage_map = heap_alloc(max_items * sizeof(uint32_t));

    __builtin_memset(p_queue, 0, sizeof(*p_queue));
    __builtin_memset(p_node_storage, 0, max_items);
    __builtin_memset(p_storage_usage_map, 0, max_items / 32);

    semaphore_init(&p_queue->num_nodes);

    p_queue->max_items = max_items;
    p_queue->item_size = item_size;
    p_queue->p_node_storage = p_node_storage;
    p_queue->p_item_storage = p_item_storage;
    p_queue->p_storage_usage_map = p_storage_usage_map;

    queue_node_t *p_dummy_node = new_node(p_queue, NULL);
    ASSERT(p_dummy_node);
    __builtin_memset(p_dummy_node, 0, sizeof(queue_node_t));

    p_queue->p_head = p_dummy_node;
    p_queue->p_tail = p_queue->p_head;
}

bool queue_write(queue_t *p_queue, void *p_data) {
    queue_node_t *p_node = new_node(p_queue, p_data);
    if (!p_node) { return false; }

    for (;;) {
        queue_node_t *p_old_tail = p_queue->p_tail;
        queue_node_t *p_null_ptr = NULL;
        if (atomic_compare_exchange_strong(&p_old_tail->p_next, &p_null_ptr,
                                           p_node)) {
            // old_tail.next has been advanced, now let's advance queue.tail.
            // Failure can occur if queue_read() has already updated the tail
            // to point at old_tail.next (that is, p_node).
            atomic_compare_exchange_strong(&p_queue->p_tail, &p_old_tail,
                                           p_node);
            break;
        } else {
            // old_tail.next is not NULL, maybe some other task was preempted
            // before advancing the tail pointer.
            atomic_compare_exchange_strong(&p_queue->p_tail, &p_old_tail,
                                           p_old_tail->p_next);
        }
    }

    semaphore_increase(&p_queue->num_nodes);
    return true;
}

void queue_read(queue_t *p_queue, void *p_buf, size_t item_size) {
    semaphore_decrease(&p_queue->num_nodes);

    for (;;) {
        queue_node_t *p_head = atomic_load(&p_queue->p_head);
        queue_node_t *p_tail = atomic_load(&p_queue->p_tail);
        queue_node_t *p_next = p_head->p_next;
        if (p_head == p_tail) {
            // The head and tail pointers are equal. Maybe the queue is empty,
            // or maybe the tail pointer has not yet been advanced.
            if (p_tail->p_next) {
                atomic_compare_exchange_strong(&p_queue->p_tail, &p_tail,
                                               p_next);
            } else {
                panic_silent();
            }
        } else {
            __builtin_memcpy(p_buf, p_next->p_data, item_size);
            if (atomic_compare_exchange_strong(&p_queue->p_head, &p_head,
                                               p_next)) {
                free_node(p_queue, p_head);
                break;
            }
        }
    }
}

static queue_node_t *new_node(queue_t *p_queue, void *p_src_data) {
    size_t map_idx = 0;
    while (map_idx < p_queue->max_items / 32) {
        uint32_t used_nodes = p_queue->p_storage_usage_map[map_idx];
        if (used_nodes == 0xFFFFFFFF) {
            map_idx++;
            continue;
        }

        uint32_t free_nodes = ~used_nodes;
        uint32_t free_node_bit = __builtin_ctz(free_nodes);
        uint32_t free_node_idx = map_idx * 32 + free_node_bit;
        uint32_t new_used_nodes = used_nodes | (1 << free_node_bit);

        if (atomic_compare_exchange_strong(
                &p_queue->p_storage_usage_map[map_idx], &used_nodes,
                new_used_nodes)) {
            void *p_stored_data;
            if (p_src_data) {
                p_stored_data = p_queue->p_item_storage +
                                free_node_idx * p_queue->item_size;
                __builtin_memcpy(p_stored_data, p_src_data, p_queue->item_size);
            } else {
                p_stored_data = NULL;
            }

            queue_node_t *p_node = &p_queue->p_node_storage[free_node_idx];
            p_node->p_data = p_stored_data;
            p_node->p_next = NULL;

            return p_node;
        }
    }
    return NULL;
}

static void free_node(queue_t *p_queue, queue_node_t *p_node) {
    size_t node_idx = p_node - p_queue->p_node_storage;
    size_t map_idx = node_idx / 32;
    size_t node_bit = node_idx % 32;
    ASSERT(map_idx < p_queue->max_items / 32);

    for (;;) {
        uint32_t used_nodes = p_queue->p_storage_usage_map[map_idx];
        uint32_t new_used_nodes = used_nodes & ~(1 << node_bit);
        if (atomic_compare_exchange_strong(
                &p_queue->p_storage_usage_map[map_idx], &used_nodes,
                new_used_nodes)) {
            break;
        }
    }
}
