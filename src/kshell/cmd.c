/**
 * @file cmd.c
 * Command handling functions of kshell.
 *
 * To add a new command:
 *
 * 1. Increment #NUM_CMDS.
 * 2. Add the command name to #gp_cmd_names at index _N_.
 * 3. Add a handler function declaration near the other command handlers.
 * 4. Add the handler function pointer to p_cmd_funs **at the same index** (_N_)
 *    in #kshell_cmd_parse().
 * 5. Add the function definition near the other command handlers.
 */

#include <cpuid.h>

#include "blkdev/ahci.h"
#include "blkdev/blkdev.h"
#include "devmgr.h"
#include "elf.h"
#include "heap.h"
#include "kprintf.h"
#include "kshell/cmd.h"
#include "kshell/kbdlog.h"
#include "kshell/vasview.h"
#include "list.h"
#include "mbi.h"
#include "panic.h"
#include "pci.h"
#include "smp.h"
#include "string.h"
#include "taskmgr.h"
#include "term.h"
#include "vmm.h"

#define NUM_CMDS 16
#define MAX_ARGS 32

static char const *const gp_cmd_names[NUM_CMDS] = {
    "clear",   "help",    "mbimap", "mbimod", "elfhdr", "exec",
    "tasks",   "vasview", "cpuid",  "devmgr", "pci",    "blkdev",
    "execrep", "kbdlog",  "heap",   "kill",
};

static uint32_t g_exec_entry;

static void cmd_clear(char **pp_args, size_t num_args);
static void cmd_help(char **pp_args, size_t num_args);
static void cmd_mbimap(char **pp_args, size_t num_args);
static void cmd_mbimod(char **pp_args, size_t num_args);
static void cmd_elfhdr(char **pp_args, size_t num_args);
static void cmd_exec(char **pp_args, size_t num_args);
static void cmd_exec_entry(void);
static void cmd_tasks(char **pp_args, size_t num_args);
static void cmd_vasview(char **pp_args, size_t num_args);
static void cmd_cpuid(char **pp_args, size_t num_args);
static void cmd_devmgr(char **pp_args, size_t num_args);
static void cmd_pci(char **pp_args, size_t num_args);
static void cmd_blkdev(char **pp_args, size_t num_args);
static void cmd_execrep(char **pp_args, size_t num_args);
static void cmd_kbdlog(char **pp_args, size_t num_args);
static void cmd_heap(char **pp_args, size_t num_args);
static void cmd_kill(char **pp_args, size_t num_args);

void kshell_cmd_parse(char const *p_cmd) {
    char *pp_args[MAX_ARGS];
    size_t num_args = string_split(p_cmd, ' ', true, pp_args, MAX_ARGS);

    if (0 == num_args) { return; }

    if (num_args > MAX_ARGS) {
        kprintf("kshell: too much arguments (more than %u)\n", MAX_ARGS);
        return;
    }

    static void (*const p_cmd_funs[NUM_CMDS])(char **pp_args,
                                              size_t num_args) = {
        cmd_clear,   cmd_help,   cmd_mbimap, cmd_mbimod,
        cmd_elfhdr,  cmd_exec,   cmd_tasks,  cmd_vasview,
        cmd_cpuid,   cmd_devmgr, cmd_pci,    cmd_blkdev,
        cmd_execrep, cmd_kbdlog, cmd_heap,   cmd_kill,
    };

    for (size_t idx = 0; idx < NUM_CMDS; idx++) {
        if (string_equals(pp_args[0], gp_cmd_names[idx])) {
            p_cmd_funs[idx](pp_args, num_args);
            return;
        }
    }

    kprintf("kshell: unknown command: '%s'\n", pp_args[0]);
}

static void cmd_clear(char **pp_args, size_t num_args) {
    if (1 != num_args) {
        kprintf("Usage: %s\n", pp_args[0]);
        return;
    }

    term_acquire_mutex();
    term_clear();
    term_release_mutex();
}

static void cmd_help(char **pp_args, size_t num_args) {
    if (1 != num_args) {
        kprintf("Usage: %s\n", pp_args[0]);
        return;
    }

    kprintf("Available commands:\n");
    for (size_t idx = 0; idx < NUM_CMDS; idx++) {
        kprintf("%s", gp_cmd_names[idx]);
        if (idx != (NUM_CMDS - 1)) { kprintf(", "); }
    }
    kprintf("\n");
}

static void cmd_mbimap(char **pp_args, size_t num_args) {
    if (1 != num_args) {
        kprintf("Usage: %s\n", pp_args[0]);
        return;
    }

    mbi_t const *p_mbi = mbi_ptr();

    if (!(p_mbi->flags & MBI_FLAG_MMAP)) {
        kprintf("No memory map in MBI\n");
        return;
    }

    uint32_t byte = 0;
    while (byte < p_mbi->mmap_length) {
        uint32_t const *p_entry = ((uint32_t const *)(p_mbi->mmap_addr + byte));

        uint32_t size = *(p_entry + 0);
        uint64_t base_addr = *((uint64_t const *)(p_entry + 1));
        uint64_t length = *((uint64_t const *)(p_entry + 3));
        uint32_t type = *(p_entry + 5);

        uint64_t end = (base_addr + length);

        kprintf("size = %d, addr = 0x%P", size, ((uint32_t)base_addr));
        if (end >> 32) {
            kprintf(", end = 0x%08X%08X", ((uint32_t)(end >> 32)),
                    ((uint32_t)end));
        } else {
            kprintf(", end = 0x%08X", ((uint32_t)end));
        }
        kprintf(", type = %d\n", type);

        byte += (4 + size);
    }
}

static void cmd_mbimod(char **pp_args, size_t num_args) {
    if (1 != num_args) {
        kprintf("Usage: %s\n", pp_args[0]);
        return;
    }

    mbi_t const *p_mbi = mbi_ptr();

    if (!(p_mbi->flags & MBI_FLAG_MODS)) {
        kprintf("No modules in MBI\n");
        return;
    }

    kprintf("Module count = %u\n", p_mbi->mods_count);

    mbi_mod_t const *p_mod = ((mbi_mod_t const *)p_mbi->mods_addr);
    for (size_t idx = 0; idx < p_mbi->mods_count; idx++) {
        kprintf("Module %d: ", idx);
        if (p_mod->string) {
            kprintf("'%s', ", p_mod->string);
        } else {
            kprintf("string = NULL, ");
        }
        kprintf("start = %P, end = %P, size = %u", p_mod->mod_start,
                p_mod->mod_end, (p_mod->mod_end - p_mod->mod_start));
        kprintf("\n");

        p_mod += 1;
    }
}

static void cmd_elfhdr(char **pp_args, size_t num_args) {
    if (1 != num_args) {
        kprintf("Usage: %s\n", pp_args[0]);
        return;
    }

    mbi_mod_t const *p_mod_user = mbi_find_mod("user");
    if (!p_mod_user) {
        kprintf("Module 'user' is not found\n");
        return;
    }

    elf_dump(p_mod_user->mod_start);
}

static void cmd_exec(char **pp_args, size_t num_args) {
    if (1 != num_args) {
        kprintf("Usage: %s\n", pp_args[0]);
        return;
    }

    mbi_t const *p_mbi = mbi_ptr();

    if (!(p_mbi->flags & MBI_FLAG_MODS)) {
        kprintf("Module 'user' is not found\n");
        return;
    }

    mbi_mod_t const *p_mod_user = mbi_find_mod("user");
    if (!p_mod_user) {
        kprintf("Module 'user' is not found\n");
        return;
    }

    uint32_t *p_dir = vmm_clone_kvas_dir();

    bool ok = elf_load(p_dir, p_mod_user->mod_start, &g_exec_entry);
    if (!ok) { kprintf("kshell: failed to load the executable\n"); }

    task_t *p_task =
        taskmgr_local_new_user_task("user", p_dir, ((uint32_t)cmd_exec_entry));
    kprintf("kshell: task id %u\n", p_task->id);
}

static void cmd_exec_entry(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    __asm__ volatile("sti");

    taskmgr_local_go_usermode(g_exec_entry);

    panic_enter();
    kprintf("kshell: call to taskmgr_go_usermode() has returned\n");
    panic("unexpected behavior");
}

static void cmd_tasks(char **pp_args, size_t num_args) {
    if (1 != num_args) {
        kprintf("Usage: %s\n", pp_args[0]);
        return;
    }

    for (uint8_t proc_num = 0; proc_num < smp_get_num_procs(); proc_num++) {
        taskmgr_t *const taskmgr = smp_get_proc(proc_num)->taskmgr;
        if (!taskmgr) {
            kprintf("kshell: no task manager for processor %u\n", proc_num);
            continue;
        }
        taskmgr_lock_scheduler(taskmgr);
    }

    kprintf("%3s  %3s  %10s  %10s  %10s  %5s  %5s  %4s\n", "ID", "CPU",
            "PAGEDIR", "ESP", "MAX ESP", "USED", "BLOCK", "TERM");

    taskmgr_lock_all_tasks_list();
    const list_t *p_all_tasks = taskmgr_all_tasks_list();
    for (list_node_t *p_node = p_all_tasks->p_first_node; p_node != NULL;
         p_node = p_node->p_next) {
        task_t *p_task =
            LIST_NODE_TO_STRUCT(p_node, task_t, all_tasks_list_node);
        kprintf("%3u  %3d  0x%08x  0x%08x  0x%08x  %5d  %5s  %4s\n", p_task->id,
                p_task->taskmgr->proc_num, p_task->tcb.page_dir_phys,
                (uint32_t)p_task->tcb.p_kernel_stack->p_top,
                (uint32_t)p_task->tcb.p_kernel_stack->p_top_max,
                (int32_t)p_task->tcb.p_kernel_stack->p_top_max -
                    (int32_t)p_task->tcb.p_kernel_stack->p_top,
                p_task->is_blocked ? "YES" : "NO",
                p_task->is_terminating ? "YES" : "NO");
    }
    taskmgr_unlock_all_tasks_list();

    for (uint8_t proc_num = 0; proc_num < smp_get_num_procs(); proc_num++) {
        taskmgr_t *const taskmgr = smp_get_proc(proc_num)->taskmgr;
        if (!taskmgr) {
            kprintf("kshell: no task manager for processor %u\n", proc_num);
            continue;
        }
        taskmgr_unlock_scheduler(taskmgr);
    }
}

static void cmd_vasview(char **pp_args, size_t num_args) {
    if (num_args > 2) {
        kprintf("Usage: %s [pgdir]\n", pp_args[0]);
        kprintf("Optional arguments:\n");
        kprintf("  pgdir  virtual address of a page directory to view"
                " (default: kshell task VAS)\n");
        return;
    }

    uint32_t pgdir;
    __asm__ volatile("mov %%cr3, %0"
                     : "=a"(pgdir)
                     : /* no inputs */
                     : /* no clobber */);

    if (2 == num_args) {
        bool ok = string_to_uint32(pp_args[1], &pgdir, 16);
        if (!ok) {
            kprintf("Invalid argument: '%s'\n", pp_args[1]);
            kprintf("pgdir must be a hexadecimal 32-bit unsigned integer\n");
            return;
        }
    }

    vasview(pgdir);
}

static void cmd_cpuid(char **pp_args, size_t num_args) {
    if (2 != num_args) {
        kprintf("Usage: %s <leaf>\n", pp_args[0]);
        return;
    }

    uint32_t leaf;
    bool b_arg_ok = string_to_uint32(pp_args[1], &leaf, 10);
    if (!b_arg_ok) {
        kprintf("Invalid argument: '%s'\n", pp_args[1]);
        kprintf("leaf must be a decimal 32-bit unsigned integer\n");
        return;
    }

    unsigned int eax, ebx, ecx, edx;
    int cpuid_ok = __get_cpuid(leaf, &eax, &ebx, &ecx, &edx);
    if (!cpuid_ok) {
        kprintf("Unsupported cpuid leaf: %u\n", leaf);
        return;
    }

    kprintf("EAX = %08x\n", eax);
    kprintf("EBX = %08x\n", ebx);
    kprintf("ECX = %08x\n", ecx);
    kprintf("EDX = %08x\n", edx);
}

static void cmd_devmgr(char **pp_args, size_t num_args) {
    if (num_args != 1) {
        kprintf("Usage: %s\n", pp_args[0]);
        return;
    }

    devmgr_iter_t dev_iter;
    devmgr_iter_init(&dev_iter, DEVMGR_CLASS_NONE);

    uint32_t idx = 0;
    devmgr_dev_t *dev;
    while ((dev = devmgr_iter_next(&dev_iter))) {
        kprintf("%u: id %u, class %s, driver %s\n", idx, dev->id,
                devmgr_class_name(dev->dev_class),
                devmgr_driver_name(dev->driver_id));
        idx++;
    }

    kprintf("%u device(s)\n", idx);
}

static void cmd_pci(char **pp_args, size_t num_args) {
    if (num_args < 2) {
        kprintf("Usage: %s <cmd> [args]\n", pp_args[0]);
        kprintf("cmd must be one of: dump, list\n");
        kprintf("args depend on cmd\n");
        return;
    }

    if (string_equals(pp_args[1], "list")) {
        if (num_args != 2) {
            kprintf("Usage: pci list\n");
            return;
        }

        for (size_t dev_idx = 0; dev_idx < pci_num_devs(); dev_idx++) {
            const pci_dev_t *dev = pci_get_dev_const(dev_idx);
            pci_dump_dev_short(dev);
        }
    } else if (string_equals(pp_args[1], "dump")) {
        if (num_args != 3) {
            kprintf("Usage: pci dump <device number>\n");
            return;
        }

        uint32_t dev_idx;
        bool b_ok = string_to_uint32(pp_args[2], &dev_idx, 10);
        if (!b_ok || dev_idx >= pci_num_devs()) {
            kprintf("Invalid argument: '%s'\n", pp_args[2]);
            kprintf("device number must be an unsigned decimal number less than"
                    " %u\n",
                    pci_num_devs());
            return;
        }

        const pci_dev_t *dev = pci_get_dev_const(dev_idx);
        pci_dump_dev_header(dev);
    } else {
        kprintf("kshell: pci: unknown subcommand: '%s'\n", pp_args[1]);
        return;
    }
}

static void cmd_blkdev(char **pp_args, size_t num_args) {
    if (num_args != 4) {
        kprintf("Usage: %s <device ID> <start sector> <num sectors>\n",
                pp_args[0]);
        return;
    }

    bool ok;
    uint32_t dev_id;
    uint32_t start_sector;
    uint32_t num_sectors;

    ok = string_to_uint32(pp_args[1], &dev_id, 10);
    if (!ok) {
        kprintf("Invalid argument: '%s'\n", pp_args[1]);
        kprintf("device ID must be a decimal 32-bit unsigned integer\n");
        return;
    }
    ok = string_to_uint32(pp_args[2], &start_sector, 10);
    if (!ok) {
        kprintf("Invalid argument: '%s'\n", pp_args[2]);
        kprintf("start sector must be a decimal 32-bit unsigned integer\n");
        return;
    }
    ok = string_to_uint32(pp_args[3], &num_sectors, 10);
    if (!ok) {
        kprintf("Invalid argument: '%s'\n", pp_args[3]);
        kprintf("num sectors must be a decimal 32-bit unsigned integer\n");
        return;
    }

    if (num_sectors == 0) { return; }

    devmgr_dev_t *const dev = devmgr_get_by_id(dev_id);
    if (!dev) {
        kprintf("kshell: no device with ID %u\n", dev_id);
        return;
    }
    if (dev->dev_class != DEVMGR_CLASS_DISK) {
        kprintf("kshell: device ID %u is %s, not a block device\n", dev_id,
                devmgr_class_name(dev->dev_class));
        return;
    }

    uint8_t *const p_buf = heap_alloc_aligned(512 * num_sectors, 2);
    kprintf("kshell: p_buf = %P\n", p_buf);

    blkdev_req_t blkdev_req = {
        .state = BLKDEV_REQ_INACTIVE,
        .op = BLKDEV_OP_READ,
        .start_sector = start_sector,
        .read_sectors = num_sectors,
        .read_buf = p_buf,
        .dev = &dev->blkdev_dev,
    };
    semaphore_init(&blkdev_req.sem_done);

    ok = blkdev_enqueue_req(&blkdev_req);
    if (!ok) {
        kprintf("kshell: request queue of blkdev is full\n");
        heap_free(p_buf);
        return;
    }

    semaphore_decrease(&blkdev_req.sem_done);
    kprintf("kshell: decreased sem_done\n");

    if (blkdev_req.state == BLKDEV_REQ_SUCCESS) {
        const size_t num_bytes = 512 * num_sectors;
        for (size_t row = 0; row < (num_bytes + 23) / 24; row++) {
            kprintf("%02x  ", row);
            for (size_t byte = 0; byte < 24; byte++) {
                if ((byte > 0) && ((byte % 8) == 0)) { kprintf(" "); }
                size_t idx = ((row * 24) + byte);
                if (idx < num_bytes) { kprintf("%02x ", p_buf[idx]); }
            }
            kprintf("\n");
        }
    } else if (blkdev_req.state == BLKDEV_REQ_ERROR) {
        kprintf("kshell: blkdev I/O error\n");
    } else {
        kprintf("kshell: blkdev request has unexpected state %u\n",
                blkdev_req.state);
    }

    heap_free(p_buf);
}

static void cmd_execrep(char **pp_args, size_t num_args) {
    if (num_args > 2) {
        kprintf("Usage: %s [repeats]\n", pp_args[0]);
        return;
    }

    char const *p_cmd = "exec";
    if (num_args == 1) {
        for (;;) {
            kshell_cmd_parse(p_cmd);
        }
    } else {
        uint32_t num_repeats = 0;
        bool b_ok = string_to_uint32(pp_args[1], &num_repeats, 10);
        if (!b_ok) {
            kprintf("repeats must be a decimal 32-bit unsigned integer\n");
            return;
        }

        for (uint32_t idx = 0; idx < num_repeats; idx++) {
            kshell_cmd_parse(p_cmd);
        }
    }
}

static void cmd_kbdlog(char **pp_args, size_t num_args) {
    if (num_args != 2) {
        kprintf("Usage: %s <number of events>\n", pp_args[0]);
        return;
    }

    uint32_t num_events;
    bool ok = string_to_uint32(pp_args[1], &num_events, 10);
    if (!ok) {
        kprintf("Invalid argument: '%s'\n", pp_args[1]);
        kprintf("number of events must be a decimal 32-bit unsigned integer\n");
        return;
    }

    kbdlog((size_t)num_events);
}

static void cmd_heap(char **pp_args, size_t num_args) {
    if (num_args != 2) {
        kprintf("Usage: %s <cmd>\n", pp_args[0]);
        kprintf("cmd must be one of: dump\n");
        return;
    }

    if (string_equals(pp_args[1], "dump")) {
        heap_dump_tags();
    } else {
        kprintf("%s: unknown command: '%s'\n", pp_args[0], pp_args[1]);
        return;
    }
}

static void cmd_kill(char **pp_args, size_t num_args) {
    if (num_args != 2) {
        kprintf("Usage: %s <task ID>\n", pp_args[0]);
        kprintf("Schedules the specified task for termination.\n");
        return;
    }

    uint32_t task_id;
    bool ok = string_to_uint32(pp_args[1], &task_id, 10);
    if (!ok) {
        kprintf("Invalid argument: '%s'\n", pp_args[1]);
        kprintf("task ID must be a decimal 32-bit unsigned integer\n");
        return;
    }

    /*
    task_t *p_task = taskmgr_get_task_by_id(task_id);
    if (p_task) {
        taskmgr_terminate_task(p_task);
        kprintf("Marked task %u for termination\n", task_id);
    } else {
        kprintf("No such task: ID %u\n", task_id);
    }
    */
}
