#pragma once

#include "isrs.h"

#define SYSCALL_INT_NUM 100

void syscall_dispatch(isr_regs_t *p_regs);
