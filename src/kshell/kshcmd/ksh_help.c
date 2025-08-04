#include "kprintf.h"
#include "kshell/ksharg.h"
#include "kshell/kshcmd/ksh_help.h"
#include "kshell/kshcmd/kshcmd.h"

static ksharg_posarg_desc_t g_ksh_help_posargs[] = {};

static ksharg_flag_desc_t g_ksh_help_flags[] = {
    {
        .short_name = "h",
        .long_name = "help",
        .help_str = "Print this message and exit.",
        .val_name = NULL,
    },
};

static const ksharg_parser_desc_t g_ksh_help_parser = {
    .name = "help",
    .description = "Kshell help.",
    .epilog = NULL,

    .num_posargs = sizeof(g_ksh_help_posargs) / sizeof(g_ksh_help_posargs[0]),
    .posargs = g_ksh_help_posargs,

    .num_flags = sizeof(g_ksh_help_flags) / sizeof(g_ksh_help_flags[0]),
    .flags = g_ksh_help_flags,
};

static void prv_ksh_help_builtins(void);

void ksh_help(list_t *arg_list) {
    ksharg_parser_inst_t *parser;
    ksharg_err_t err;

    err = ksharg_inst_parser(&g_ksh_help_parser, &parser);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_help: error instantiating the argument parser: %u\n", err);
        return;
    }

    err = ksharg_parse_list(parser, arg_list);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_help: error parsing arguments: %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }

    bool do_help;

    ksharg_flag_inst_t *flag_help;
    err = ksharg_get_flag_inst(parser, "help", &flag_help);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_help: error getting flag 'help': %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }
    do_help = flag_help->given_str;

    if (do_help) {
        ksharg_print_help(&g_ksh_help_parser);
    } else {
        kprintf("kshell - interactive kernel shell\n");
        prv_ksh_help_builtins();
    }

    ksharg_free_parser_inst(parser);
}

static void prv_ksh_help_builtins(void) {
    kprintf("Built-in commands:\n");

    const kshell_cmd_t *cmds;
    size_t num_cmds;
    kshcmd_get_cmds(&cmds, &num_cmds);

    for (size_t idx = 0; idx < num_cmds; idx++) {
        const kshell_cmd_t *const cmd = &cmds[idx];
        kprintf("%2u. %10s - %s\n", 1 + idx, cmd->name, cmd->help_str);
    }

    kprintf("These built-ins may accept arguments. Pass '-h' (or '--help') to "
            "a command you\nwant to run to get its help message.\n");
}
