#pragma once

#include <stdint.h>

void taskmgr_init(void);
void taskmgr_start_scheduler(__attribute__((noreturn))
                             void (* p_init_entry)(void));

void taskmgr_schedule(void);

void taskmgr_new_user_task(uint32_t * p_dir, uint32_t entry);
void taskmgr_go_usermode(uint32_t entry);

uint32_t taskmgr_running_task_id(void);
void     taskmgr_dump_tasks(void);
