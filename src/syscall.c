#include "kprintf.h"
#include "syscall.h"
#include "taskmgr.h"

static void syscall_sleep_ms(uint32_t duration_ms);

void syscall_dispatch(isr_regs_t *p_regs) {
    uint32_t num = p_regs->eax;
    uint32_t arg1 = p_regs->ecx;

    if (num == SYSCALL_SLEEP_MS) {
        syscall_sleep_ms(arg1);
    } else {
        kprintf("syscall: task %u, unknown num %u\n",
                taskmgr_running_task()->id, num);
    }
}

static void syscall_sleep_ms(uint32_t duration_ms) {
    kprintf("syscall: task %u, sleep %u ms\n", taskmgr_running_task()->id,
            duration_ms);
    taskmgr_sleep_ms(duration_ms);
}
