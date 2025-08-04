/**
 * @file cmd.c
 * Command handling for kshell.
 */

#include <cpuid.h>

#include "assert.h"
#include "kprintf.h"
#include "kshell/cmd.h"
#include "kshell/cmd/ksh_clear.h"
#include "kshell/cmd/ksh_devmgr.h"
#include "kshell/cmd/ksh_help.h"
#include "kshell/cmd/ksh_mbi.h"
#include "kshell/cmd/ksh_taskmgr.h"
#include "kshell/cmd/ksh_vasview.h"
#include "kshell/kshscan.h"
#include "kstring.h"

static const kshell_cmd_t g_kshell_cmds[] = {
    // clang-format off
    // NOTE: these should be sorted alphabetically, so that the 'help' command
    // lists them in alphabetical order.
    {"devmgr", ksh_devmgr, "device manager"},
    {"clear", ksh_clear, "clear the terminal"},
    {"help", ksh_help, "kshell help"},
    {"mbi", ksh_mbi, "view the Multiboot Information structure"},
    {"taskmgr", ksh_taskmgr, "task manager"},
    {"vasview", ksh_vasview, "inspect virtual address spaces"},
    // clang-format on
};

static const kshell_cmd_t *prv_kshell_cmd_find(const char *name);

void kshell_cmd_parse(const char *p_cmd) {
    list_t arg_list;
    list_init(&arg_list, NULL);

    kshscan_err_t err = kshscan_str(p_cmd, &arg_list);
    if (err.err_type != KSHSCAN_ERR_NONE) {
        kprintf("kshell: failed to parse '%s': error %u at char %u\n", p_cmd,
                err.err_type, err.char_pos);
        return;
    }

    const list_node_t *const arg0_node = list_pop_first(&arg_list);
    if (!arg0_node) {
        kshscan_free_arg_list(&arg_list);
        return;
    }

    const char *const arg0_str =
        LIST_NODE_TO_STRUCT(arg0_node, kshscan_arg_t, list_node)->arg_str;

    const kshell_cmd_t *const cmd = prv_kshell_cmd_find(arg0_str);
    if (cmd) {
        ASSERT(cmd->f_handler);
        cmd->f_handler(&arg_list);
    } else {
        kprintf("kshell: unrecognized command '%s'\n", arg0_str);
    }

    kshscan_free_arg_list(&arg_list);
}

void kshell_get_cmds(const kshell_cmd_t **out_cmds, size_t *out_num_cmds) {
    if (out_cmds) { *out_cmds = g_kshell_cmds; }
    if (out_num_cmds) {
        *out_num_cmds = sizeof(g_kshell_cmds) / sizeof(g_kshell_cmds[0]);
    }
}

static const kshell_cmd_t *prv_kshell_cmd_find(const char *name) {
    const size_t num_cmds = sizeof(g_kshell_cmds) / sizeof(g_kshell_cmds[0]);
    for (size_t idx = 0; idx < num_cmds; idx++) {
        const kshell_cmd_t *const cmd = &g_kshell_cmds[idx];
        if (string_equals(name, cmd->name)) { return cmd; }
    }
    return NULL;
}
