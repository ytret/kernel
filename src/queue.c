/*
 * LIFO queue.
 */

#include <stdatomic.h>

#include "heap.h"
#include "panic.h"
#include "queue.h"
#include "semaphore.h"

void queue_init(queue_t *p_queue) {
    __builtin_memset(p_queue, 0, sizeof(*p_queue));
    semaphore_init(&p_queue->sem_num_items);

    queue_item_t *p_dummy_item = heap_alloc(sizeof(queue_item_t));
    __builtin_memset(p_dummy_item, 0, sizeof(queue_item_t));
    p_queue->p_head = p_dummy_item;
    p_queue->p_tail = p_queue->p_head;
}

void queue_write(queue_t *p_queue, queue_item_t *p_item) {
    for (;;) {
        queue_item_t *p_old_tail = atomic_load(&p_queue->p_tail);
        queue_item_t *p_null_ptr = NULL;
        if (atomic_compare_exchange_strong(&p_old_tail->p_next, &p_null_ptr,
                                           p_item)) {
            // old_tail.next has been advanced, now let's advance queue.tail.
            // Failure can occur if queue_read() has already updated the tail
            // to point at old_tail.next (that is, p_item).
            atomic_compare_exchange_strong(&p_queue->p_tail, &p_old_tail,
                                           p_item);
            break;
        } else {
            // old_tail.next is not NULL, maybe some other task was preempted
            // before advancing the tail pointer.
            atomic_compare_exchange_strong(&p_queue->p_tail, &p_old_tail,
                                           p_old_tail->p_next);
        }
    }

    semaphore_increase(&p_queue->sem_num_items);
}

void queue_read(queue_t *p_queue, void *p_buf, size_t item_size) {
    semaphore_decrease(&p_queue->sem_num_items);

    for (;;) {
        queue_item_t *p_head = atomic_load(&p_queue->p_head);
        queue_item_t *p_tail = atomic_load(&p_queue->p_tail);
        queue_item_t *p_next = p_head->p_next;
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
            __builtin_memcpy(p_buf, p_next, item_size);
            if (atomic_compare_exchange_strong(&p_queue->p_head, &p_head,
                                               p_next)) {
                heap_free(p_head);
                break;
            }
        }
    }
}
