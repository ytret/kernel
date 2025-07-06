#include <stdatomic.h>
#include <stddef.h>

#include "list.h"
#include "semaphore.h"
#include "taskmgr.h"

void semaphore_init(semaphore_t *p_sem) {
    __builtin_memset(p_sem, 0, sizeof(*p_sem));
    list_init(&p_sem->waiting_tasks, NULL);
}

void semaphore_increase(semaphore_t *p_sem) {
    atomic_fetch_add(&p_sem->count, 1);

    list_node_t *const waiting_node = list_pop_first(&p_sem->waiting_tasks);
    if (waiting_node) {
        task_t *const waiting_task =
            LIST_NODE_TO_STRUCT(waiting_node, task_t, list_node);
        taskmgr_unblock(waiting_task);
    }
}

void semaphore_decrease(semaphore_t *p_sem) {
    for (;;) {
        const int old_count = atomic_load(&p_sem->count);
        if (old_count > 0) {
            const int new_count = old_count - 1;
            if (atomic_compare_exchange_weak(&p_sem->count, &old_count,
                                             new_count)) {
                break;
            }
        } else {
            taskmgr_block_running_task(&p_sem->waiting_tasks);
            taskmgr_local_reschedule();
        }
    }
}
