/**
 * @file cmd.c
 * Command handling for kshell.
 */

#include <cpuid.h>

#include "kprintf.h"
#include "kshell/cmd.h"
#include "kshell/cmd/ksh_mbi.h"
#include "kshell/kshscan.h"
#include "kstring.h"

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

    kprintf("kshell: unrecognized command '%s'\n", arg0_str);

    kshscan_free_arg_list(&arg_list);
}
