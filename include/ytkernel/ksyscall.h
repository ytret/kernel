#pragma once

#include <stdint.h>

#define KSYSCALL_SLEEP_MS  0
#define KSYSCALL_EXIT      1
#define KSYSCALL_MAX_CALLS 256

typedef struct {
    uint32_t call_id;
    uintptr_t arg1;
    uintptr_t arg2;
    uintptr_t arg3;
    uintptr_t arg4;
    uintptr_t arg5;
} ksyscall_args_t;

int ksyscall_dispatch(ksyscall_args_t *args);
