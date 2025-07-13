#include <stdatomic.h>
#include <stddef.h>

#include "ksemaphore.h"
#include "list.h"
#include "taskmgr.h"

void semaphore_init(semaphore_t *sem) {
    __builtin_memset(sem, 0, sizeof(*sem));
    list_init(&sem->waiting_tasks, NULL);
}

void semaphore_increase(semaphore_t *sem) {
    // Locking the waiting tasks list for the duration of the whole function
    // body guarantees that when semaphore_decrease() acquires the list lock, it
    // sees that 'count' is not zero.
    spinlock_acquire(&sem->list_lock);

    atomic_fetch_add(&sem->count, 1);
    list_node_t *const waiting_node = list_pop_first(&sem->waiting_tasks);
    if (waiting_node) {
        task_t *const waiting_task =
            LIST_NODE_TO_STRUCT(waiting_node, task_t, list_node);
        taskmgr_unblock(waiting_task);
    }

    spinlock_release(&sem->list_lock);
}

void semaphore_decrease(semaphore_t *sem) {
    for (;;) {
        int old_count = atomic_load(&sem->count);
        if (old_count > 0) {
            const int new_count = old_count - 1;
            if (atomic_compare_exchange_weak(&sem->count, &old_count,
                                             new_count)) {
                break;
            }
        } else {
            spinlock_acquire(&sem->list_lock);

            // See mutex_acquire() for the reason behind this second check. In
            // short, it prevents a missed task wake-up.
            old_count = atomic_load(&sem->count);
            if (old_count > 0) {
                spinlock_release(&sem->list_lock);
                continue;
            }

            taskmgr_block_running_task(&sem->waiting_tasks);
            spinlock_release(&sem->list_lock);

            taskmgr_local_reschedule();
        }
    }
}
