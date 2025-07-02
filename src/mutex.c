#include "mutex.h"
#include "panic.h"
#include "taskmgr.h"

void mutex_init(task_mutex_t *mutex) {
    __builtin_memset(mutex, 0, sizeof(*mutex));
    list_init(&mutex->waiting_tasks, NULL);
}

void mutex_acquire(task_mutex_t *mutex) {
    /*
     * This function needs to work in several kernel states: pre-SMP, no-SMP,
     * and after-SMP.
     *
     * In pre-SMP, hence pre-multitasking, there are no task managers and only
     * one running processor. The mutex acquiral always succeeds.
     *
     * In no-SMP, after multitasking is initialized, there is only one running
     * processor, but the mutex may be locked by one of the tasks.
     *
     * In after-SMP, there are several processors running their own task
     * managers. The mutex may be locked by a task running on another processor,
     * hence controlled by a different task manager.
     */

    task_t *const caller_task = taskmgr_local_running_task();
    if (caller_task && mutex->locking_task == caller_task) { panic_silent(); }

    if (__sync_bool_compare_and_swap(&mutex->locking_task, NULL, caller_task)) {
        // The caller task has successfuly acquired the mutex.
        if (caller_task) { caller_task->num_owned_mutexes++; }
    } else if (caller_task) {
        // The mutex is blocked by another task. In no-SMP and after-SMP, force
        // a rescheduling.
        taskmgr_block_running_task(&mutex->waiting_tasks);
        taskmgr_reschedule();
    } else {
        // There is no caller task, meaning this is the pre-SMP state. The mutex
        panic_silent();
    }
}

void mutex_release(task_mutex_t *mutex) {
    // See the comment in mutex_acquire() for explanation of what pre-SMP,
    // no-SMP, and after-SMP mean.

    task_t *const caller_task = taskmgr_local_running_task();
    if (caller_task) {
        // There is a locally running task, hence the kernel is either in the
        // no-SMP or after-SMP state. Lock the local scheduler so that the
        // caller task is not preempted.
        taskmgr_local_lock_scheduler();
    }

    if (caller_task) {
        if (mutex->locking_task == caller_task) {
            caller_task->num_owned_mutexes--;
        } else {
            // The locally running task tried to release a kernel mutex that it
            // has not acquired. The kernel code is broken.
            panic_silent();
        }
    }

    list_node_t *const waiting_node = list_pop_first(&mutex->waiting_tasks);
    if (waiting_node) {
        // There is a waiting task. This means that the mutex has been
        // previously acquired, implying the kernel is in the no-SMP or
        // after-SMP state.
        if (!caller_task) { panic_silent(); }

        // The top waiting task may be assigned to the local task manager or to
        // the task manager of another processor.
        task_t *const waiting_task =
            LIST_NODE_TO_STRUCT(waiting_node, task_t, list_node);
        mutex->locking_task = waiting_task;
        taskmgr_unblock(waiting_task);
    } else {
        mutex->locking_task = NULL;
    }

    if (caller_task) { taskmgr_local_unlock_scheduler(); }
}

bool mutex_caller_owns(task_mutex_t *mutex) {
    task_t *const caller_task = taskmgr_local_running_task();
    if (caller_task) {
        // There is a caller task, which is only possible in the no-SMP or
        // after-SMP states.
        return mutex->locking_task == caller_task;
    } else {
        // There is no caller task, which is only possible in the pre-SMP state.
        // There is no way to distinguish if the mutex is held or not held.
        return true;
    }
}
