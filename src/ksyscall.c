#include "kerr.h"
#include "ksyscall.h"
#include "taskmgr.h"

typedef int (*ksyscall_fn)(const ksyscall_args_t *args);

static int prv_ksyscall_sleep_ms(const ksyscall_args_t *args);
static int prv_ksyscall_exit(const ksyscall_args_t *args);

static const ksyscall_fn g_ksyscall_table[KSYSCALL_MAX_CALLS] = {
    [KSYSCALL_SLEEP_MS] = prv_ksyscall_sleep_ms,
    [KSYSCALL_EXIT] = prv_ksyscall_exit,
};

int ksyscall_dispatch(ksyscall_args_t *args) {
    if (args->call_id >= KSYSCALL_MAX_CALLS) { return -KERR_NO_SYSCALL; }
    const ksyscall_fn call_handler = g_ksyscall_table[args->call_id];
    if (!call_handler) { return -KERR_NO_SYSCALL; }
    return call_handler(args);
}

static int prv_ksyscall_sleep_ms(const ksyscall_args_t *args) {
    const uint32_t duration_ms = (uint32_t)args->arg1;
    taskmgr_local_sleep_ms(duration_ms);
    return 0;
}

static int prv_ksyscall_exit(const ksyscall_args_t *args) {
    (void)args;
    // taskmgr_terminate_task(taskmgr_running_task());
    return 0;
}
