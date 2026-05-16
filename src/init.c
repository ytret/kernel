/**
 * @file init.c
 * SMP-aware initial tasks implementation.
 */

#include "blkdev/blkdev.h"
#include "config.h"
#include "devmgr.h"
#include "init.h"
#include "kshell/kshell.h"
#include "log.h"
#include "panic.h"
#include "smp.h"
#include "taskmgr.h"

#include "arch/x86/apic/lapic.h"

#ifdef YTKERNEL_ENABLE_TESTS
#include "test/ktest.h"
#endif

[[gnu::noreturn]]
void init_bsp_task(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    __asm__ volatile("sti");

    lapic_init_tim(LAPIC_TIM_PERIOD_MS);
    smp_set_bsp_ready();

#ifdef YTKERNEL_ENABLE_TESTS
    ktest_run_stage(KTEST_SMP);
    ktest_end();
#endif

    taskmgr_local_new_kernel_task("blkdev", (uint32_t)blkdev_task_entry);

    LOG_FLOW("waiting for blkdev...");
    while (!blkdev_is_ready()) {}
    LOG_FLOW("blkdev task is ready for requests");

    devmgr_init_blkdev_parts();

    kshell();

    PANIC("kshell returned");
}

[[gnu::noreturn]]
void init_ap_task(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    __asm__ volatile("sti");

    lapic_init_tim(LAPIC_TIM_PERIOD_MS);
    smp_set_ap_ready();
    while (!smp_is_bsp_ready()) {
        __asm__ volatile("pause" ::: "memory");
    }

#ifdef YTKERNEL_ENABLE_TESTS
    spinlock_init(&smp_get_running_proc()->ktest_lock);
    for (;;) {
        ktest_smpjob_do_local_job();
        __asm__ volatile("pause");
    }
#endif

    for (;;) {
        __asm__ volatile("hlt");
    }
}
