/**
 * @file init.c
 * SMP-aware initial tasks implementation.
 */

#include "blkdev/blkdev.h"
#include "init.h"
#include "kprintf.h"
#include "kshell/kshell.h"
#include "panic.h"
#include "taskmgr.h"

[[gnu::noreturn]]
void init_bsp_task(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    __asm__ volatile("sti");

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

    for (;;) {
        __asm__ volatile("hlt");
    }
}
