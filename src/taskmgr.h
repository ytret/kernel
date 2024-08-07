#pragma once

#include <stdint.h>

#include "list.h"
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
    bool b_is_blocked;

    list_node_t list_node;
} task_t;

typedef struct {
    // The task that has acquired the mutex.
    task_t *p_locking_task;

    // List of tasks waiting to acquire the lock.
    list_t waiting_tasks;
} task_mutex_t;

void taskmgr_init(void);
void taskmgr_start_scheduler(
    __attribute__((noreturn)) void (*p_init_entry)(void));

void taskmgr_schedule(void);
void taskmgr_lock_scheduler(void);
void taskmgr_unlock_scheduler(void);

task_t *taskmgr_running_task(void);
list_t *taskmgr_runnable_tasks(void);
void taskmgr_new_user_task(uint32_t *p_dir, uint32_t entry);

void taskmgr_go_usermode(uint32_t entry);
void taskmgr_acquire_mutex(task_mutex_t *p_mutex);
void taskmgr_release_mutex(task_mutex_t *p_mutex);
bool taskmgr_owns_mutex(task_mutex_t *p_mutex);
