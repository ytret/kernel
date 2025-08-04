#include "kprintf.h"
#include "kshell/ksharg.h"
#include "kshell/kshcmd/ksh_clear.h"
#include "term.h"

static ksharg_posarg_desc_t g_ksh_clear_posargs[] = {};

static ksharg_flag_desc_t g_ksh_clear_flags[] = {
    {
        .short_name = "h",
        .long_name = "help",
        .help_str = "Print this message and exit.",
        .val_name = NULL,
    },
};

static const ksharg_parser_desc_t g_ksh_clear_parser = {
    .name = "clear",
    .description = "Clear the terminal.",
    .epilog = NULL,

    .num_posargs = sizeof(g_ksh_clear_posargs) / sizeof(g_ksh_clear_posargs[0]),
    .posargs = g_ksh_clear_posargs,

    .num_flags = sizeof(g_ksh_clear_flags) / sizeof(g_ksh_clear_flags[0]),
    .flags = g_ksh_clear_flags,
};

void ksh_clear(list_t *arg_list) {
    ksharg_parser_inst_t *parser;
    ksharg_err_t err;

    err = ksharg_inst_parser(&g_ksh_clear_parser, &parser);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_clear: error instantiating the argument parser: %u\n",
                err);
        return;
    }

    err = ksharg_parse_list(parser, arg_list);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_clear: error parsing arguments: %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }

    bool do_help;

    ksharg_flag_inst_t *flag_help;
    err = ksharg_get_flag_inst(parser, "help", &flag_help);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_clear: error getting flag 'help': %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }
    do_help = flag_help->given_str;

    if (do_help) {
        ksharg_print_help(&g_ksh_clear_parser);
    } else {
        term_acquire_mutex();
        term_clear();
        term_release_mutex();
    }

    ksharg_free_parser_inst(parser);
}
