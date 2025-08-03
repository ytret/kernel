#include "assert.h"
#include "kprintf.h"
#include "ksh_taskmgr.h"
#include "kshell/ksharg.h"
#include "kstring.h"
#include "smp.h"
#include "taskmgr.h"

static ksharg_posarg_desc_t g_ksh_taskmgr_posargs[] = {};

static ksharg_flag_desc_t g_ksh_taskmgr_flags[] = {
    {
        .short_name = "h",
        .long_name = "help",
        .help_str = "Print this message and exit.",
        .val_name = NULL,
    },
    {
        .short_name = "k",
        .long_name = "kill",
        .help_str = "Kill a task.",
        .val_name = "ID",
        .def_val_str = NULL,
    },
    {
        .short_name = "l",
        .long_name = "list",
        .help_str = "List tasks.",
        .val_name = NULL,
    },
};

static const ksharg_parser_desc_t g_ksh_taskmgr_parser = {
    .name = "taskmgr",
    .description = "Task manager.",
    .epilog = NULL,

    .num_posargs =
        sizeof(g_ksh_taskmgr_posargs) / sizeof(g_ksh_taskmgr_posargs[0]),
    .posargs = g_ksh_taskmgr_posargs,

    .num_flags = sizeof(g_ksh_taskmgr_flags) / sizeof(g_ksh_taskmgr_flags[0]),
    .flags = g_ksh_taskmgr_flags,
};

static void prv_ksh_taskmgr_list(void);
static void prv_ksh_taskmgr_kill(const char *id_str);

void ksh_taskmgr(list_t *arg_list) {
    ksharg_parser_inst_t *parser;
    ksharg_err_t err;

    err = ksharg_inst_parser(&g_ksh_taskmgr_parser, &parser);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_taskmgr: error instantiating the argument parser: %u\n",
                err);
        return;
    }

    err = ksharg_parse_list(parser, arg_list);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_taskmgr: error parsing arguments: %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }

    bool do_help;
    bool do_kill;
    const char *kill_id_str;
    bool do_list;

    ksharg_flag_inst_t *flag_help;
    err = ksharg_get_flag_inst(parser, "help", &flag_help);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_taskmgr: error getting flag 'help': %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }
    do_help = flag_help->given_str;

    ksharg_flag_inst_t *flag_kill;
    err = ksharg_get_flag_inst(parser, "kill", &flag_kill);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_taskmgr: error getting flag 'kill': %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }
    do_kill = flag_kill->given_str;
    kill_id_str = flag_kill->val_str;

    ksharg_flag_inst_t *flag_list;
    err = ksharg_get_flag_inst(parser, "list", &flag_list);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_taskmgr: error getting flag 'list': %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }
    do_list = flag_list->given_str;

    if (do_list && do_kill) {
        kprintf("ksh_taskmgr: flags '--list' and '--kill' cannot be used "
                "together\n");
        ksharg_free_parser_inst(parser);
        return;
    }

    if (do_help) {
        ksharg_print_help(&g_ksh_taskmgr_parser);
        ksharg_free_parser_inst(parser);
    } else if (do_kill) {
        prv_ksh_taskmgr_kill(kill_id_str);
    } else if (do_list) {
        prv_ksh_taskmgr_list();
    } else {
        kprintf("ksh_taskmgr: no action specified\n");
    }

    ksharg_free_parser_inst(parser);
}

static void prv_ksh_taskmgr_list(void) {
    for (uint8_t proc_num = 0; proc_num < smp_get_num_procs(); proc_num++) {
        taskmgr_t *const taskmgr = smp_get_proc(proc_num)->taskmgr;
        if (!taskmgr) {
            kprintf("ksh_taskmgr: no task manager for processor %u\n",
                    proc_num);
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
            kprintf("ksh_taskmgr: no task manager for processor %u\n",
                    proc_num);
            continue;
        }
        taskmgr_unlock_scheduler(taskmgr);
    }
}

static void prv_ksh_taskmgr_kill(const char *id_str) {
    ASSERT(id_str);

    uint32_t id;
    if (!string_to_uint32(id_str, &id, 10)) {
        kprintf("ksh_taskmgr: bad integer '%u'\n", id_str);
        return;
    }

    task_t *const task = taskmgr_get_task_by_id(id);
    if (task) {
        taskmgr_terminate_task(task);
        kprintf("ksh_taskmgr: marked task ID %u for termination\n", id);
    } else {
        kprintf("ksh_taskmgr: no task with ID %u\n", id);
    }
}
