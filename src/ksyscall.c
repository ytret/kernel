#include "ksyscall.h"
#include "taskmgr.h"

void syscall_sleep_ms(uint32_t duration_ms) {
    taskmgr_local_sleep_ms(duration_ms);
}

void syscall_exit(uint32_t exit_code) {
    (void)exit_code;
    // taskmgr_terminate_task(taskmgr_running_task());
}
