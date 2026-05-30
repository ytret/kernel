#include "kerr.h"
#include "ksyscall.h"
#include "taskmgr.h"

static int prv_ksyscall_sleep_ms(ksyscall_args_t *args);
static int prv_ksyscall_exit(ksyscall_args_t *args);

int ksyscall_dispatch(ksyscall_args_t *args) {
    if (args->call_id == SYSCALL_SLEEP_MS) {
        return prv_ksyscall_sleep_ms(args);
    } else if (args->call_id == SYSCALL_EXIT) {
        return prv_ksyscall_exit(args);
    }

    return -KERR_NO_SYSCALL;
}

static int prv_ksyscall_sleep_ms(ksyscall_args_t *args) {
    const uint32_t duration_ms = (uint32_t)args->arg1;
    taskmgr_local_sleep_ms(duration_ms);
    return 0;
}

static int prv_ksyscall_exit(ksyscall_args_t *args) {
    (void)args;
    // taskmgr_terminate_task(taskmgr_running_task());
    return 0;
}
