#pragma once

#include "isrs.h"

#define SYSCALL_INT_NUM 100
#define SYSCALL_SLEEP_MS 0

void syscall_dispatch(isr_regs_t *p_regs);
