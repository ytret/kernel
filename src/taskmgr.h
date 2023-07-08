#pragma once

#include <gdt.h>
#include <stack.h>

typedef struct
__attribute__ ((packed))
{
    uint32_t page_dir_phys;
    stack_t * p_kernel_stack;
} tcb_t;

void taskmgr_init(void);
void taskmgr_schedule(void);

void taskmgr_switch_tasks(tcb_t * p_from, tcb_t const * p_to, tss_t * p_tss);
