/**
 * @file init.c
 * SMP-aware initial tasks implementation.
 */

#include "acpi/lapic.h"
#include "assert.h"
#include "blkdev/blkdev.h"
#include "devmgr.h"
#include "fs/ramfs.h"
#include "init.h"
#include "kprintf.h"
#include "kshell/kshell.h"
#include "panic.h"
#include "smp.h"
#include "taskmgr.h"
#include "term.h"
#include "vfs/vfs.h"
#include "vfs/vfs_fs.h"

[[gnu::noreturn]]
void init_bsp_task(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    __asm__ volatile("sti");

    lapic_init_tim(LAPIC_TIM_PERIOD_MS);
    smp_set_bsp_ready();

    taskmgr_local_new_kernel_task("term", (uint32_t)term_task);
    taskmgr_local_new_kernel_task("blkdev", (uint32_t)blkdev_task_entry);

    kprintf("init_bsp_task: waiting for blkdev...\n");
    while (!blkdev_is_ready()) {}
    kprintf("init_bsp_task: blkdev task is ready for requests\n");

    devmgr_init_blkdev_parts();

    vfs_init();
    const vfs_fs_desc_t *const fs_ramfs = ramfs_get_desc();
    ramfs_ctx_t *const ramfs = ramfs_init(1024);

    vfs_err_t err = fs_ramfs->f_mount(ramfs, vfs_root_node());
    kprintf("err = %u\n", err);
    ASSERT(err == VFS_ERR_NONE);

    vfs_node_t *new_node;
    err = vfs_root_node()->ops->f_mknode(vfs_root_node(), &new_node, "abc",
                                         VFS_NODE_FILE);
    kprintf("err = %u\n", err);
    ASSERT(err == VFS_ERR_NONE);

    vfs_dirent_t dirents[10];
    size_t num_dirents;
    err = vfs_root_node()->ops->f_readdir(vfs_root_node(), dirents,
                                          sizeof(dirents) / sizeof(dirents[0]),
                                          &num_dirents);
    kprintf("err = %u\n", err);
    ASSERT(err == VFS_ERR_NONE);

    for (size_t idx = 0; idx < num_dirents; idx++) {
        vfs_dirent_t *dirent = &dirents[idx];
        kprintf("%u. %s\n", idx, dirent->name);
    }

    kshell();

    panic_enter();
    kprintf("init: init_bsp_task: kshell returned\n");
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
