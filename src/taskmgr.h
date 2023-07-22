#pragma once

#include <stdint.h>

void taskmgr_init(void);
void taskmgr_new_user_task(uint32_t * p_dir, uint32_t entry);

void taskmgr_schedule(void);

uint32_t taskmgr_running_task_id(void);
void     taskmgr_dump_tasks(void);
