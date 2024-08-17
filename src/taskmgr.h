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
    uint64_t sleep_until_counter_ms;

    list_node_t list_node;
} task_t;

void taskmgr_init(__attribute__((noreturn)) void (*p_init_entry)(void));

void taskmgr_schedule(void);
void taskmgr_lock_scheduler(void);
void taskmgr_unlock_scheduler(void);

task_t *taskmgr_running_task(void);
list_t *taskmgr_runnable_tasks(void);
void taskmgr_new_user_task(uint32_t *p_dir, uint32_t entry);

void taskmgr_go_usermode(uint32_t entry);
void taskmgr_sleep_ms(uint32_t duration_ms);
void taskmgr_block_running_task(list_t *p_task_list);
void taskmgr_unblock(task_t *p_task);
