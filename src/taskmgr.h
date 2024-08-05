#pragma once

#include <stdint.h>

#include "stack.h"

typedef struct __attribute__((packed)) {
    // This field order is relied upon by taskmgr_switch_tasks() assembly
    // function (see taskmgr.s).

    uint32_t page_dir_phys;
    stack_t *p_kernel_stack;
} tcb_t;

typedef struct task {
    uint32_t id;
    stack_t kernel_stack;
    tcb_t tcb;

    struct task *p_next;
} task_t;

typedef struct {
    // The task that has acquired the mutex.
    task_t *p_locking_task;

    // List of tasks waiting to acquire the lock.
    task_t *p_first_waiting_task;

    // The most recent task that has failed to acquire the lock.
    // NOTE: this is a valid pointer only when `p_first_waiting_task` is
    // non-NULL.
    task_t *p_last_waiting_task;
} task_mutex_t;

void taskmgr_init(void);
void taskmgr_start_scheduler(
    __attribute__((noreturn)) void (*p_init_entry)(void));

void taskmgr_schedule(void);
void taskmgr_new_user_task(uint32_t *p_dir, uint32_t entry);

void taskmgr_go_usermode(uint32_t entry);
void taskmgr_acquire_mutex(task_mutex_t *p_mutex);
void taskmgr_release_mutex(task_mutex_t *p_mutex);

void taskmgr_dump_tasks(void);
