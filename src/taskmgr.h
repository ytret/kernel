/**
 * @file taskmgr.h
 * Task manager API.
 */

#pragma once

#include <stdint.h>

#include "list.h"
#include "spinlock.h"
#include "stack.h"

typedef struct taskmgr taskmgr_t;

/**
 * Thread control block.
 * @warning
 * This field order is relied upon by taskmgr_switch_tasks() assembly function
 * (see taskmgr.s).
 */
typedef struct [[gnu::packed]] {
    uint32_t page_dir_phys;
    stack_t *p_kernel_stack;
} tcb_t;

/**
 * Task context.
 */
typedef struct {
    uint32_t id;
    taskmgr_t *taskmgr;
    stack_t kernel_stack;
    tcb_t tcb;

    /**
     * Blocked flag.
     * If a task is blocked (e.g., by a mutex), it must not appear on the
     * runnable tasks list and cannot be terminated.
     */
    bool is_blocked;
    /**
     * Sleeping flag.
     * Once #task_t.sleep_until_counter_ms is reached, the task is unblocked.
     */
    bool is_sleeping;

    /**
     * Termination flag.
     * This task is to be terminated when it is switched from after being run.
     * See #taskmgr_schedule() for details.
     */
    bool is_terminating;

    /**
     * Number of owned mutexes.
     * If a task owns any mutex, it cannot be terminated.
     * This value is managed by #mutex_acquire(), #mutex_release().
     */
    size_t num_owned_mutexes;
    /**
     * Target counter value to unblock a sleeping task at.
     * Relevant only if #task_t.is_sleeping is `true`.
     */
    uint64_t sleep_until_counter_ms;

    /**
     * Node in the task lists.
     * Each task (except the currently running one)_ is always in only one of
     * the following lists:
     * - @ref g_runnable_tasks "Runnable tasks"
     * - @ref g_sleeping_tasks "Sleeping tasks"
     * - @ref task_mutex_t.waiting_tasks "Mutex waiting tasks"
     */
    list_node_t list_node;

    /// Node in @ref g_all_tasks "the list of all tasks".
    list_node_t all_tasks_list_node;
} task_t;

struct taskmgr {
    /**
     * Nested locks preventing @ref taskmgr_schedule "task scheduling".
     * @note
     * Initial value must be 1 and it is decremented after the scheduler is
     * initialized in #taskmgr_init().
     */
    int scheduler_lock;

    /**
     * Running task.
     * This value can be used to get the current running task in an IRQ handler.
     */
    task_t *running_task;

    /**
     * List of tasks that the running task can switch to (node:
     * #task_t.list_node). The tasks in this list are not sleeping and are not
     * blocked.
     */
    list_t runnable_tasks;

    /**
     * List of sleeping tasks (node: #task_t.list_node).
     * See #taskmgr_sleep_ms().
     */
    list_t sleeping_tasks;

    /**
     * List of all tasks (node: #task_t.all_tasks_list_node).
     * - Tasks are added to this list upon creation in #new_task().
     * - Tasks are removed from this list only by the #deleter_task().
     */
    list_t all_tasks;

    spinlock_t runnable_tasks_lock;
    spinlock_t sleeping_tasks_lock;
    spinlock_t all_tasks_lock;

    /**
     * Idle task.
     * The idle task is always present in the runnable tasks list and provides
     * the scheduler a task to switch to, when there are no other runnable
     * tasks.
     */
    task_t *idle_task;
    /**
     * Deleter task.
     * The deleter task is responsible for deletion of tasks that are marked for
     * termination (see #task_t.is_terminating). It is switched to only when the
     * running task is being terminated and has no locks.
     */
    task_t *deleter_task;
    /**
     * Initial task.
     * This is the first kernel-mode task that is responsible for initializing
     * the system. Cf. _init_ on Unix-based systems.
     */
    task_t *init_task;

    /**
     * Parameter for the deleter task provided by the scheduler.
     * It holds a pointer to the task to be deleted.
     */
    task_t *task_to_delete;
};

/**
 * Starts the scheduler and runs @ref gp_init_task "the initial task".
 *
 * This function does not return, for the initial task entry must not and does
 * not return.
 *
 * @warning
 * @a p_init_entry() must enable interrupts, otherwise no task switches will
 * happen.
 */
[[gnu::noreturn]]
void taskmgr_local_init([[gnu::noreturn]] void (*p_init_entry)(void));

/**
 * Performs a scheduling step.
 * It is intented, but this is not necessary, to be called inside an ISR context
 * -- either in the timer IRQ ISR, or in the syscall ISR.
 */
void taskmgr_local_schedule(void);

/**
 * Forces a scheduling step inside or outside of an ISR context.
 * Can be called in ordinary kernel tasks when a resource is blocked and
 * rescheduling is required.
 */
void taskmgr_local_reschedule(void);

void taskmgr_lock_scheduler(taskmgr_t *taskmgr);
void taskmgr_unlock_scheduler(taskmgr_t *taskmgr);

task_t *taskmgr_running_task(taskmgr_t *taskmgr);

void taskmgr_local_lock_scheduler(void);
void taskmgr_local_unlock_scheduler(void);
task_t *taskmgr_local_running_task(void);

/**
 * Returns the context of the task with ID @a task_id.
 * @param task_id Task ID to search for.
 * @returns
 * - Pointer to the task with ID @a task_id, if found.
 * - `NULL` otherwise.
 */
task_t *taskmgr_get_task_by_id(taskmgr_t *taskmgr, uint32_t task_id);

/**
 * Creates a new runnable kernel-mode task with a mapped userspace stack.
 *
 * The difference from #taskmgr_new_kernel_task() is that this function maps
 * the user stack. The initial entry point is reached in kernel-mode. See
 * #taskmgr_go_usermode().
 *
 * @param p_dir Page directory to be used by the task.
 * @param entry Task entry point.
 * @returns Task context pointer. The task is in the runnable tasks list.
 */
task_t *taskmgr_local_new_user_task(uint32_t *p_dir, uint32_t entry);

/**
 * Creates a new runnable kernel-mode task.
 * @param entry Task entry point.
 * @returns Task context pointer. The task is in the runnable tasks list.
 */
task_t *taskmgr_local_new_kernel_task(uint32_t entry);

/**
 * Does a far return (_iret_) with usermode segments to address @a entry.
 * @param entry Userspace entry point.
 * @note Code at address @a entry is executed in usermode.
 */
void taskmgr_go_usermode(uint32_t entry);

/**
 * Blocks the execution of the running task for @a duration_ms milliseconds.
 * From the caller task point of view, this function returns after @a
 * duration_ms milliseconds have passed.
 * @param duration_ms Duration of sleep (ms).
 */
void taskmgr_local_sleep_ms(uint32_t duration_ms);

/**
 * Marks the task @a p_task as terminating.
 * See #task_t.is_terminating.
 * @param p_task Task to mark as terminating (passing the running task pointer
 *               is allowed).
 */
void taskmgr_local_terminate_task(task_t *p_task);

/**
 * Blocks the running task and appends it to the @a task_list list.
 * See #task_t.is_blocked.
 * @param task_list Task list to append the running task to.
 * @warning
 * This function returns immediately. Call #taskmgr_reschedule() to force a
 * scheduling step.
 */
void taskmgr_block_running_task(list_t *task_list);

/**
 * Unblocks the task @a p_task and appends it to the runnable tasks list.
 * @param p_task Task to unblock (must be blocked, see
 *               #taskmgr_block_running_task()).
 * @warning
 * Do not call this function on a non-blocked task.
 */
void taskmgr_unblock(task_t *task);
