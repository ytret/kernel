/**
 * @file init.c
 * SMP-aware initial tasks implementation.
 */

#include "acpi/lapic.h"
#include "blkdev/blkdev.h"
#include "init.h"
#include "kprintf.h"
#include "kshell/kshell.h"
#include "panic.h"
#include "smp.h"
#include "taskmgr.h"

[[gnu::noreturn]]
void init_bsp_task(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    __asm__ volatile("sti");
    lapic_init_tim(LAPIC_TIM_PERIOD_MS);
    smp_set_bsp_ready();

    taskmgr_local_new_kernel_task((uint32_t)blkdev_task_entry);

    kshell();

    panic_enter();
    kprintf("init_entry: kshell returned\n");
    panic("unexpected behavior");
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

    for (;;) {
        __asm__ volatile("hlt");
    }
}
