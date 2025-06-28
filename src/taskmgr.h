/**
 * @file taskmgr.h
 * Task manager API.
 */

#pragma once

#include <stdint.h>

#include "list.h"
#include "stack.h"

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
void taskmgr_init([[gnu::noreturn]] void (*p_init_entry)(void));

/**
 * Performs a scheduling step.
 * It is intented, but this is not necessary, to be called inside an ISR context
 * -- either in the timer IRQ ISR, or in the syscall ISR.
 */
void taskmgr_schedule(void);
/**
 * Forces a scheduling step inside or outside an ISR context.
 * Can be called in ordinary kernel tasks when a resource is blocked and
 * rescheduling is required.
 */
void taskmgr_reschedule(void);
/// Locks the scheduler. See #g_scheduler_lock.
void taskmgr_lock_scheduler(void);
/// Decreases the nested scheduler lock. See #g_scheduler_lock.
void taskmgr_unlock_scheduler(void);

task_t *taskmgr_running_task(void);
list_t *taskmgr_all_tasks_list(void);
/**
 * Returns the context of the task with ID @a task_id.
 * @param task_id Task ID to search for.
 * @returns
 * - Pointer to the task with ID @a task_id, if found.
 * - `NULL` otherwise.
 */
task_t *taskmgr_get_task_by_id(uint32_t task_id);

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
task_t *taskmgr_new_user_task(uint32_t *p_dir, uint32_t entry);
/**
 * Creates a new runnable kernel-mode task.
 * @param entry Task entry point.
 * @returns Task context pointer. The task is in the runnable tasks list.
 */
task_t *taskmgr_new_kernel_task(uint32_t entry);

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
void taskmgr_sleep_ms(uint32_t duration_ms);

/**
 * Marks the task @a p_task as terminating.
 * See #task_t.is_terminating.
 * @param p_task Task to mark as terminating (passing the running task pointer
 *               is allowed).
 */
void taskmgr_terminate_task(task_t *p_task);

/**
 * Blocks the running task and appends it to the @a p_task_list list.
 * See #task_t.is_blocked.
 * @param p_task_list Task list to append the running task to.
 * @warning
 * This function returns immediately. Call #taskmgr_reschedule() to force a
 * scheduling step.
 */
void taskmgr_block_running_task(list_t *p_task_list);
/**
 * Unblocks the task @a p_task and appends it to the runnable tasks list.
 * @param p_task Task to unblock (must be blocked, see
 *               #taskmgr_block_running_task()).
 * @warning
 * Do not call this function on a non-blocked task.
 */
void taskmgr_unblock(task_t *p_task);
