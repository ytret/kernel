#include "mutex.h"
#include "panic.h"
#include "taskmgr.h"

void mutex_acquire(task_mutex_t *p_mutex) {
    task_t *p_caller_task = taskmgr_running_task();
    if (p_caller_task && mutex_caller_owns(p_mutex)) { panic_silent(); }

    if (__sync_bool_compare_and_swap(&p_mutex->p_locking_task, NULL,
                                     p_caller_task)) {
        // Caller task has successfuly acquired the mutex.
    } else {
        // The mutex is blocked by another task.
        taskmgr_block_running_task(&p_mutex->waiting_tasks);
        taskmgr_schedule();
    }
}

void mutex_release(task_mutex_t *p_mutex) {
    task_t *p_caller_task = taskmgr_running_task();
    taskmgr_lock_scheduler();

    if (p_caller_task && !mutex_caller_owns(p_mutex)) { panic_silent(); }

    list_node_t *p_waiting_task_node = list_pop_first(&p_mutex->waiting_tasks);
    if (p_caller_task && p_waiting_task_node) {
        task_t *p_waiting_task =
            LIST_NODE_TO_STRUCT(p_waiting_task_node, task_t, list_node);
        p_mutex->p_locking_task = p_waiting_task;
        taskmgr_unblock(p_waiting_task);
    } else {
        p_mutex->p_locking_task = NULL;
    }

    taskmgr_unlock_scheduler();
}

bool mutex_caller_owns(task_mutex_t *p_mutex) {
    return p_mutex->p_locking_task == taskmgr_running_task();
}
