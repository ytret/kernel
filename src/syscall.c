#include "syscall.h"
#include "taskmgr.h"

static void syscall_sleep_ms(uint32_t duration_ms);
static void syscall_exit(uint32_t exit_code);

void syscall_dispatch(isr_regs_t *p_regs) {
    uint32_t num = p_regs->eax;
    uint32_t arg1 = p_regs->ecx;

    if (num == SYSCALL_SLEEP_MS) {
        syscall_sleep_ms(arg1);
    } else if (num == SYSCALL_EXIT) {
        syscall_exit(arg1);
    }
}

static void syscall_sleep_ms(uint32_t duration_ms) {
    taskmgr_sleep_ms(duration_ms);
}

static void syscall_exit(uint32_t exit_code) {
    (void)exit_code;
    // taskmgr_terminate_task(taskmgr_running_task());
}
