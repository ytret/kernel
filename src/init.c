/**
 * @file init.c
 * SMP-aware initial tasks implementation.
 */

#include "assert.h"
#include "blkdev/blkdev.h"
#include "devmgr.h"
#include "fs/ramfs.h"
#include "heap.h"
#include "init.h"
#include "kprintf.h"
#include "log.h"
#include "panic.h"
#include "smp.h"
#include "taskmgr.h"
#include "term.h"
#include "vfs/file.h"
#include "vfs/vnode.h"

#include "arch/x86/apic/lapic.h"

[[gnu::noreturn]]
void init_bsp_task(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    __asm__ volatile("sti");

    lapic_init_tim(LAPIC_TIM_PERIOD_MS);
    smp_set_bsp_ready();

    taskmgr_local_new_kernel_task("term", (uint32_t)term_task);
    taskmgr_local_new_kernel_task("blkdev", (uint32_t)blkdev_task_entry);

    LOG_FLOW("waiting for blkdev...");
    while (!blkdev_is_ready()) {}
    LOG_FLOW("blkdev task is ready for requests");

    devmgr_init_blkdev_parts();

    vnode_root_init();

    vnode_t *const root_node = vnode_root_node();
    ASSERT(root_node);

    ramfs_ctx_t *ramfs = ramfs_init(1024);
    ASSERT(ramfs);
    const vfs_fs_desc_t *ramfs_desc = ramfs_get_desc();

    vfs_err_t vfs_err = ramfs_desc->f_mount(ramfs, root_node);
    if (vfs_err) { PANIC("mount err %d: %s", vfs_err, vfs_err_str(vfs_err)); }

    vnode_t *foo_node = NULL;
    vfs_err = ramfs_node_mknode(root_node, &foo_node, "foo", VNODE_FILE);
    if (vfs_err) { PANIC("mknode err %d: %s", vfs_err, vfs_err_str(vfs_err)); }

    file_t file = {0};
    file.flags = FILE_RDONLY;
    file_err_t err;

    err = file_open_path_str("/foo", &file);
    if (err != FILE_ERR_NONE) { PANIC("open error %d", err); }

    size_t buf_size = 16;
    void *buf = heap_alloc(buf_size);
    size_t num_read = 0;
    err = file_read(&file, buf, buf_size, &num_read);
    if (err != FILE_ERR_NONE) { PANIC("read error %d", err); }

    LOG_INFO("num_read = %zu", num_read);
    size_t max_dump_size = 4096;
    char *dump = heap_alloc(max_dump_size);
    size_t dump_offset = 0;
    for (size_t idx = 0; idx < num_read; idx++) {
        dump_offset +=
            ksnprintf(&dump[dump_offset], max_dump_size - dump_offset, "%02x ",
                      ((uint8_t *)buf)[idx]);
    }
    LOG_INFO("%s", dump);

    err = file_close(&file);
    if (err != FILE_ERR_NONE) { PANIC("close error %d", err); }

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

    for (;;) {
        __asm__ volatile("hlt");
    }
}
