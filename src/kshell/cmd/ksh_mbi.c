#include "kprintf.h"
#include "kshell/cmd/ksh_mbi.h"
#include "kshell/ksharg.h"
#include "kstring.h"
#include "mbi.h"

static ksharg_posarg_desc_t g_ksh_mbi_posargs[] = {
    {
        .name = "type",
        .help_str = "Type of information to show (one of: help, map, mod).",
        .val_type = KSHARG_VAL_STR,
        .default_val = {.val_str = "help"},
        .required = false,
    },
};

static ksharg_flag_desc_t g_ksh_mbi_flags[] = {
    {
        .short_name = "h",
        .long_name = "help",
        .help_str = "Print this message and exit.",
        .has_val = false,
    },
};

static const ksharg_parser_desc_t g_ksh_mbi_parser = {
    .name = "mbi",
    .description = "Prints the Multiboot Information structure provided by the "
                   "bootloader.",
    .epilog = NULL,

    .num_posargs = sizeof(g_ksh_mbi_posargs) / sizeof(g_ksh_mbi_posargs[0]),
    .posargs = g_ksh_mbi_posargs,

    .num_flags = sizeof(g_ksh_mbi_flags) / sizeof(g_ksh_mbi_flags[0]),
    .flags = g_ksh_mbi_flags,
};

static void prv_ksh_mbi_print_map(void);
static void prv_ksh_mbi_print_mods(void);

void ksh_mbi(list_t *arg_list) {
    ksharg_parser_inst_t *parser;
    ksharg_err_t err;

    err = ksharg_inst_parser(&g_ksh_mbi_parser, &parser);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_mbi: error instantiating the argument parser: %u\n", err);
        return;
    }

    err = ksharg_parse_list(parser, arg_list);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_mbi: error parsing arguments: %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }

    ksharg_posarg_inst_t *arg_type;
    err = ksharg_get_posarg_inst(parser, "type", &arg_type);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_mbi: error getting argument 'type': %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }

    const char *type_str = arg_type->val.val_str;

    ksharg_flag_inst_t *flag_help;
    err = ksharg_get_flag_inst(parser, "help", &flag_help);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_mbi: error getting flag 'help': %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }

    if (flag_help->given_str || string_equals(type_str, "help")) {
        ksharg_print_help(&g_ksh_mbi_parser);
        ksharg_free_parser_inst(parser);
        return;
    }

    if (string_equals(type_str, "map")) {
        prv_ksh_mbi_print_map();
    } else if (string_equals(type_str, "mod")) {
        prv_ksh_mbi_print_mods();
    } else {
        kprintf("mbi: unrecognized type '%s'\n", type_str);
    }

    ksharg_free_parser_inst(parser);
}

static void prv_ksh_mbi_print_map(void) {
    const mbi_t *mbi = mbi_ptr();

    if (!(mbi->flags & MBI_FLAG_MMAP)) {
        kprintf("mbi: no memory map in the MBI\n");
        return;
    }

    uint32_t offset = 0;
    while (offset < mbi->mmap_length) {
        const uint32_t *const mmap_entry =
            (const uint32_t *)(mbi->mmap_addr + offset);

        const uint32_t size = *(mmap_entry + 0);
        const uint64_t base_addr = *((const uint64_t *)(mmap_entry + 1));
        const uint64_t length = *((const uint64_t *)(mmap_entry + 3));
        const uint32_t type = *(mmap_entry + 5);

        const uint64_t end = base_addr + length;

        kprintf("0x%08X%08X", (uint32_t)(base_addr >> 32), (uint32_t)base_addr);
        kprintf("..0x%08X%08X", (uint32_t)(end >> 32), (uint32_t)end);
        kprintf(" (%8u KiB)", length / 1024);
        kprintf(", type %u\n", type);

        offset += 4 + size;
    }
}

static void prv_ksh_mbi_print_mods(void) {
    const mbi_t *mbi = mbi_ptr();

    if (!(mbi->flags & MBI_FLAG_MODS) || mbi->mods_count == 0) {
        kprintf("mbi: no modules in the MBI\n");
        return;
    }

    const mbi_mod_t *mod = (const mbi_mod_t *)mbi->mods_addr;
    for (uint32_t idx = 0; idx < mbi->mods_count; idx++) {
        if (mod->string) {
            kprintf("'%s'", mod->string);
        } else {
            kprintf("(null)");
        }

        kprintf(" at 0x%08X", mod->mod_start);
        kprintf("..0x%08X", mod->mod_end);
        kprintf(" (%8u KiB)\n", (mod->mod_end - mod->mod_start) / 1024);

        mod++;
    }
}
