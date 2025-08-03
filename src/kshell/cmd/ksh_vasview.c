#include "kprintf.h"
#include "kshell/cmd/ksh_vasview.h"
#include "kshell/ksharg.h"
#include "kshell/vasview.h"
#include "kstring.h"
#include "vmm.h"

static ksharg_posarg_desc_t g_ksh_vasview_posargs[] = {
    {
        .name = "pagedir",
        .help_str = "Virtual address of a page directory to traverse.",
        .val_type = KSHARG_VAL_STR,
        .default_val = {.val_str = "kernel"},
        .required = false,
    },
};

static ksharg_flag_desc_t g_ksh_vasview_flags[] = {
    {
        .short_name = "h",
        .long_name = "help",
        .help_str = "Print this message and exit.",
        .has_val = false,
    },
};

static const ksharg_parser_desc_t g_ksh_vasview_parser = {
    .name = "vasview",
    .description = "Interactive view of page directory and page table entries.",
    .epilog = NULL,

    .num_posargs =
        sizeof(g_ksh_vasview_posargs) / sizeof(g_ksh_vasview_posargs[0]),
    .posargs = g_ksh_vasview_posargs,

    .num_flags = sizeof(g_ksh_vasview_flags) / sizeof(g_ksh_vasview_flags[0]),
    .flags = g_ksh_vasview_flags,
};

void ksh_vasview(list_t *arg_list) {
    ksharg_parser_inst_t *parser;
    ksharg_err_t err;

    err = ksharg_inst_parser(&g_ksh_vasview_parser, &parser);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_vasview: error instantiating the argument parser: %u\n",
                err);
        return;
    }

    err = ksharg_parse_list(parser, arg_list);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_vasview: error parsing arguments: %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }

    ksharg_flag_inst_t *flag_help;
    err = ksharg_get_flag_inst(parser, "help", &flag_help);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_vasview: error getting flag 'help': %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }
    if (flag_help->given_str) {
        ksharg_print_help(&g_ksh_vasview_parser);
        ksharg_free_parser_inst(parser);
        return;
    }

    ksharg_posarg_inst_t *arg_pagedir;
    err = ksharg_get_posarg_inst(parser, "pagedir", &arg_pagedir);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_vasview: error getting argument 'pagedir': %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }

    uint32_t pagedir;

    if (string_equals(arg_pagedir->val.val_str, "kernel")) {
        pagedir = (uint32_t)vmm_kvas_dir();
    } else {
        const char *arg_num_str = arg_pagedir->val.val_str;
        int arg_base = 10;
        if (string_len(arg_pagedir->val.val_str) > 2 &&
            arg_pagedir->val.val_str[0] == '0' &&
            arg_pagedir->val.val_str[1] == 'x') {
            arg_num_str = &arg_pagedir->val.val_str[2];
            arg_base = 16;
        }

        if (!string_to_uint32(arg_num_str, &pagedir, arg_base)) {
            kprintf("ksh_vasview: invalid page directory address '%s'\n",
                    arg_pagedir->val.val_str);
            ksharg_free_parser_inst(parser);
            return;
        }
    }

    vasview(pagedir);

    ksharg_free_parser_inst(parser);
}
