/**
 * @file mutex.c
 * Mutex implementation.
 *
 * Mutex acquire and release functions need to work both before multitasking and
 * multiprocessing is reached, and after they are reached.
 *
 * Before multitasking and multiprocessing there is only one "task" and ISR
 * handlers getting called at random times. The mutex is always available.
 *
 * Once each processor is initialized, i.e. @ref smp_sync "has a task manager",
 * multiple tasks on different processors may try to acquire the mutex
 * simultaneously.
 */

#include "mutex.h"
#include "panic.h"
#include "taskmgr.h"

void mutex_init(task_mutex_t *mutex) {
    __builtin_memset(mutex, 0, sizeof(*mutex));
    list_init(&mutex->waiting_tasks, NULL);
    spinlock_init(&mutex->list_lock);
}

void mutex_acquire(task_mutex_t *mutex) {
    task_t *const caller_task = taskmgr_local_running_task();
    if (caller_task && mutex->locking_task == caller_task) { panic_silent(); }

    // First, fast attempt to get the lock.
    if (__sync_bool_compare_and_swap(&mutex->locking_task, NULL, caller_task)) {
        // The mutex has been acquired. Also see below for another path that
        // leads to mutex acquisition.
        if (caller_task) { caller_task->num_owned_mutexes++; }
        return;
    }

    if (!caller_task) {
        // There is no caller task, meaning this is the pre-SMP state. The mutex
        // is locked by some task, which is only possible if some other
        // processor's task has acquired it. The processors initialization has
        // not been synchronized properly, see @ref smp_sync.
        panic_silent();
    }

    // The mutex is blocked by another task.
    spinlock_acquire(&mutex->list_lock);

    // The fast CAS-lock attempt above has failed, but at this point the locking
    // task might have released the lock, checked the list, saw it empty, and
    // left the lock released. We need to do a second CAS-lock attempt.
    if (__sync_bool_compare_and_swap(&mutex->locking_task, NULL, caller_task)) {
        // Indeed, the lock has been released and now we own it, but we also own
        // the waiting list lock, which guarantees that the previous mutex owner
        // has returned from mutex_release().
        caller_task->num_owned_mutexes++;
        spinlock_release(&mutex->list_lock);
        return;
    }

    // The lock is still owned by someone. Maybe it's a new owner which was
    // also trying to acquire the mutex and has succeeded in the above if
    // branch. Or maybe it's the same owner. In both cases, when they release
    // the lock, they will see the caller task in the waiting list.

    taskmgr_block_running_task(&mutex->waiting_tasks);

    spinlock_release(&mutex->list_lock);
    taskmgr_local_reschedule();
}

void mutex_release(task_mutex_t *mutex) {
    // Releasing the lock always leads to accessing the waiting tasks list, so
    // lock it here.
    spinlock_acquire(&mutex->list_lock);

    task_t *const caller_task = taskmgr_local_running_task();
    if (caller_task) { taskmgr_local_lock_scheduler(); }

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
        if (!caller_task) {
            // There is a task waiting, but no calling task.
            panic_silent();
        }

        // The top waiting task may be assigned to the local task manager or to
        // the task manager of another processor.
        task_t *const waiting_task =
            LIST_NODE_TO_STRUCT(waiting_node, task_t, list_node);

        if (!__sync_bool_compare_and_swap(&mutex->locking_task, caller_task,
                                          waiting_task)) {
            // Failed to atomically change 'locking_task', even though
            // mutex_acquire() must fail to set 'locking_task' if it's not NULL.
            panic_silent();
        }

        waiting_task->num_owned_mutexes++;
        taskmgr_unblock(waiting_task);
    } else {
        mutex->locking_task = NULL;
    }

    if (caller_task) { taskmgr_local_unlock_scheduler(); }
    spinlock_release(&mutex->list_lock);
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
