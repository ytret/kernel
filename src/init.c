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
#include "term.h"

#include "arch/x86/apic/lapic.h"

#ifdef YTKERNEL_ENABLE_TESTS
#include "test/ktest.h"
#include "test/vmctl.h"
#endif

[[gnu::noreturn]]
void init_bsp_task(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    __asm__ volatile("sti");

    lapic_init_tim(LAPIC_TIM_PERIOD_MS);
    smp_set_bsp_ready();

#ifdef YTKERNEL_ENABLE_TESTS
    ktest_run_stage(KTEST_SMP);

    int ktest_exitcode;
    if (ktest_should_exit_at_end(&ktest_exitcode)) {
        const ktest_globalctx_t *const ktest_ctx = ktest_get_globalctx();
        const int ratio_x1000 =
            ktest_ctx->tests_passed * 1000 / ktest_ctx->tests_run;

        LOG_INFO("ktest results:");
        LOG_INFO("%zu.%zu%% tests passed, %zu failed out of %zu",
                 ratio_x1000 / (size_t)10, ratio_x1000 % (size_t)10,
                 ktest_ctx->tests_failed, ktest_ctx->tests_run);
        vmctl_exit(ktest_exitcode);
    } else {
        taskmgr_local_init(init_bsp_task);
    }
#endif

    taskmgr_local_new_kernel_task("term", (uint32_t)term_task);
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
    for (;;) {
        ktest_smpjob_do_local_job();
        __asm__ volatile("pause");
    }
#endif

    for (;;) {
        __asm__ volatile("hlt");
    }
}
