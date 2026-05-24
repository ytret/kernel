#pragma once

#include <stdint.h>

#define SYSCALL_INT_NUM  100
#define SYSCALL_SLEEP_MS 0
#define SYSCALL_EXIT     1

void syscall_sleep_ms(uint32_t duration_ms);
void syscall_exit(uint32_t exit_code);
