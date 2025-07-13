/**
 * @file taskmgr.h
 * Task manager API.
 *
 * Each processor has its own task management. The only thing shared between the
 * task managers is the next new task ID (see #g_new_task_id).
 *
 * The functions are separated into three categories:
 * - "Global" functions operating on all task managers or data shared between
 *   all task managers.
 * - "Local" functions operating on the "local" task manager (i.e., the task
 *   manager of the running processor).
 * - Operating on the given task manager context (possibly, a task manager
 *   of another processor).
 *
 * If SMP is enabled, the data structures of a given task manager may be
 * simultaneously accessed and modified by different processors. To preclude
 * this, a scheduler lock and task list spinlocks are used.
 */

#pragma once

#include <stdint.h>

#include "list.h"
#include "spinlock.h"
#include "stack.h"

#define TASK_NAME_LEN 32

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
    /// Kernel-level unique task ID.
    uint32_t id;

    /// Name of the task.
    char name[TASK_NAME_LEN];

    /// Task manager responsible for this task.
    taskmgr_t *taskmgr;

    /// Kernel stack used while executing kernel-mode code of the task.
    stack_t kernel_stack;

    /// Thread control block used when switching to/from the task.
    tcb_t tcb;

    /**
     * Blocked flag.
     * If `true`, the task cannot be switched to, until it is unblocked by a
     * mutex release or semaphore increase operation.
     * @note
     * If a task is blocked (e.g., by a mutex), it must not appear on the
     * runnable tasks list and cannot be terminated.
     */
    bool is_blocked;

    /**
     * Sleeping flag.
     * If `true`, the task cannot be switched to, until the timer counter value
     * of #task_t.sleep_until_counter_ms is reached, then the task is unblocked.
     */
    bool is_sleeping;

    /**
     * Termination flag.
     * This task is to be terminated when it is switched from after being run.
     * See #taskmgr_local_schedule() for details.
     */
    _Atomic bool is_terminating;

    /**
     * Number of owned mutexes.
     * This value is managed by #mutex_acquire(), #mutex_release().
     * @note
     * If the task owns any mutexes, it cannot be terminated.
     */
    _Atomic size_t num_owned_mutexes;

    /**
     * Target counter value to unblock the sleeping task at.
     * Relevant only if #task_t.is_sleeping is `true`.
     */
    uint64_t sleep_until_counter_ms;

    /**
     * Node in the task lists.
     * Each task (except the currently running one) is always in only one of the
     * following lists:
     * - Runnable tasks
     * - Sleeping tasks
     * - @ref task_mutex_t.waiting_tasks "Mutex waiting tasks"
     */
    list_node_t list_node;

    /// Node in @ref g_taskmgr_all_tasks "the list of all tasks".
    list_node_t all_tasks_list_node;
} task_t;

struct taskmgr {
    /**
     * Kernel number of the processor owning this task manager.
     * See #smp_proc_t.taskmgr.
     */
    uint8_t proc_num;

    /**
     * Nested locks preventing @ref taskmgr_local_schedule "task scheduling".
     * @note
     * Initial value must be 1 and it is decremented after the scheduler is
     * initialized in #taskmgr_local_init().
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
     * See #taskmgr_local_sleep_ms().
     */
    list_t sleeping_tasks;

    spinlock_t runnable_tasks_lock;
    spinlock_t sleeping_tasks_lock;

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
 * Initializes global data structures used by per-processor task managers.
 */
void taskmgr_global_init(void);

/**
 * Returns the global list of all tasks.
 * See #g_taskmgr_all_tasks.
 * @warning
 * Lock the list with #taskmgr_lock_all_tasks_list() and unlock it with
 * #taskmgr_unlock_all_tasks_list().
 */
const list_t *taskmgr_all_tasks_list(void);

/**
 * Locks the global list of all tasks, preventing it from being modified.
 * See #g_taskmgr_all_tasks, #g_taskmgr_all_tasks_lock.
 * @warning
 * A call to this function *must* happen after the call to
 * #taskmgr_global_init().
 */
void taskmgr_lock_all_tasks_list(void);

/**
 * Unlocks the global list of all tasks.
 * See #g_taskmgr_all_tasks, #g_taskmgr_all_tasks_lock.
 * @warning
 * A call to this function *must* happen after the call to
 * #taskmgr_global_init().
 */
void taskmgr_unlock_all_tasks_list(void);

/**
 * Starts the scheduler and runs @ref taskmgr_t.init_task "the initial
 * task".
 *
 * This function does not return, for the initial task entry must not and
 * does not return.
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

/**
 * Increments the lock counter of the local task manager.
 * See #taskmgr_lock_scheduler().
 */
void taskmgr_local_lock_scheduler(void);

/**
 * Decrements the lock counter of the local task manager.
 * See #taskmgr_lock_scheduler().
 */
void taskmgr_local_unlock_scheduler(void);

/**
 * Returns the task being executed by the current processor.
 * See #taskmgr_running_task().
 */
task_t *taskmgr_local_running_task(void);

/**
 * Creates a new runnable kernel-mode task with a mapped userspace stack.
 *
 * The difference from #taskmgr_local_new_kernel_task() is that this function
 * maps the user stack. The initial entry point is reached in kernel-mode. See
 * #taskmgr_local_go_usermode().
 *
 * The created task runs on the current processor.
 *
 * @name  name  Task name (maximum length #TASK_NAME_LEN counting NUL).
 * @param p_dir Page directory to be used by the task.
 * @param entry Task entry point.
 *
 * @returns Task context pointer. The task is in the runnable tasks list.
 */
task_t *taskmgr_local_new_user_task(const char *name, uint32_t *p_dir,
                                    uint32_t entry);

/**
 * Creates a new runnable kernel-mode task.
 *
 * The created task runs on the current processor.
 *
 * @param name  Task name (maximum length #TASK_NAME_LEN counting NUL).
 * @param entry Task entry point.
 *
 * @returns Task context pointer. The task is in the runnable tasks list.
 */
task_t *taskmgr_local_new_kernel_task(const char *name, uint32_t entry);

/**
 * Does a far return (_iret_) with usermode segments to address @a entry.
 * @param entry Userspace entry point.
 * @note Code at address @a entry is executed in usermode.
 */
void taskmgr_local_go_usermode(uint32_t entry);

/**
 * Blocks the execution of the running task for @a duration_ms milliseconds.
 * From the caller task point of view, this function returns after @a
 * duration_ms milliseconds have passed.
 * @param duration_ms Duration of sleep (ms).
 */
void taskmgr_local_sleep_ms(uint32_t duration_ms);

/**
 * Blocks the running task and appends it to the @a task_list list.
 * See #task_t.is_blocked.
 *
 * @param task_list Task list to append the running task to.
 *
 * @warning
 * Guard @a task_list with a spinlock, otherwise multiple processors may try to
 * access it simultaneously causing a race condition.
 *
 * @warning
 * This function returns immediately. Call #taskmgr_local_reschedule() to force
 * a scheduling step.
 */
void taskmgr_block_running_task(list_t *task_list);

/**
 * Unblocks the task @a p_task and appends it to the runnable tasks list.
 * @param task Task to unblock (must be blocked, see
 *             #taskmgr_block_running_task()).
 * @warning
 * Do not call this function on a non-blocked task.
 */
void taskmgr_unblock(task_t *task);

/**
 * Marks the task @a task as terminating.
 * See #task_t.is_terminating.
 * @param task Task to mark as terminating (passing the running task pointer
 *             is allowed).
 */
void taskmgr_terminate_task(task_t *task);

/**
 * Increments the lock counter of @a taskmgr, preventing it from scheduling.
 * @param taskmgr Task manager context.
 */
void taskmgr_lock_scheduler(taskmgr_t *taskmgr);

/**
 * Decrements the lock counter of @a taskmgr, possibly allowing to reschedule.
 * @param taskmgr Task manager context.
 */
void taskmgr_unlock_scheduler(taskmgr_t *taskmgr);

/**
 * Returns the running task of @a taskmgr.
 * @param taskmgr Task manager context.
 */
task_t *taskmgr_running_task(taskmgr_t *taskmgr);

/**
 * Returns the context of the task with ID @a task_id.
 * @param task_id Task ID to search for.
 * @returns
 * - Pointer to the task with ID @a task_id, if found.
 * - `NULL` otherwise.
 */
task_t *taskmgr_get_task_by_id(uint32_t task_id);
