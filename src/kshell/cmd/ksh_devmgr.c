#include "devmgr.h"
#include "kprintf.h"
#include "kshell/cmd/ksh_devmgr.h"
#include "kshell/ksharg.h"
#include "kstring.h"
#include "pci.h"

static ksharg_posarg_desc_t g_ksh_devmgr_posargs[] = {};

static ksharg_flag_desc_t g_ksh_devmgr_flags[] = {
    {
        .short_name = "h",
        .long_name = "help",
        .help_str = "Print this message and exit.",
        .val_name = NULL,
    },
    {
        .short_name = "l",
        .long_name = "list",
        .help_str = "List devices registered within the kernel.",
        .val_name = NULL,
    },
    {
        .short_name = NULL,
        .long_name = "list-pci",
        .help_str = "List PCI devices.",
        .val_name = NULL,
    },
    {
        .short_name = NULL,
        .long_name = "dump-pci",
        .help_str = "Dump PCI device header.",
        .val_name = "ID",
        .def_val_str = NULL,
    },
};

static const ksharg_parser_desc_t g_ksh_devmgr_parser = {
    .name = "devmgr",
    .description = "Device manager.",
    .epilog = NULL,

    .num_posargs =
        sizeof(g_ksh_devmgr_posargs) / sizeof(g_ksh_devmgr_posargs[0]),
    .posargs = g_ksh_devmgr_posargs,

    .num_flags = sizeof(g_ksh_devmgr_flags) / sizeof(g_ksh_devmgr_flags[0]),
    .flags = g_ksh_devmgr_flags,
};

static void prv_ksh_devmgr_list(void);
static void prv_ksh_devmgr_list_pci(void);
static void prv_ksh_devmgr_dump_pci(const char *id_str);

void ksh_devmgr(list_t *arg_list) {
    ksharg_parser_inst_t *parser;
    ksharg_err_t err;

    err = ksharg_inst_parser(&g_ksh_devmgr_parser, &parser);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_devmgr: error instantiating the argument parser: %u\n",
                err);
        return;
    }

    err = ksharg_parse_list(parser, arg_list);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_devmgr: error parsing arguments: %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }

    bool do_help;
    bool do_list;
    bool do_list_pci;
    bool do_dump_pci;
    const char *pci_id_str;

    ksharg_flag_inst_t *flag_help;
    err = ksharg_get_flag_inst(parser, "help", &flag_help);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_devmgr: error getting flag 'help': %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }
    do_help = flag_help->given_str;

    ksharg_flag_inst_t *flag_list;
    err = ksharg_get_flag_inst(parser, "list", &flag_list);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_devmgr: error getting flag 'list': %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }
    do_list = flag_list->given_str;

    ksharg_flag_inst_t *flag_list_pci;
    err = ksharg_get_flag_inst(parser, "list-pci", &flag_list_pci);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_devmgr: error getting flag 'list-pci': %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }
    do_list_pci = flag_list_pci->given_str;

    ksharg_flag_inst_t *flag_dump_pci;
    err = ksharg_get_flag_inst(parser, "dump-pci", &flag_dump_pci);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_devmgr: error getting flag 'dump-pci': %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }
    do_dump_pci = flag_dump_pci->given_str;
    pci_id_str = flag_dump_pci->val_str;

    if (do_help) {
        ksharg_print_help(&g_ksh_devmgr_parser);
        ksharg_free_parser_inst(parser);
        return;
    }

    if (1 != (int)do_list + (int)do_list_pci + (int)do_dump_pci) {
        kprintf("ksh_devmgr: no action specified\n");
        ksharg_free_parser_inst(parser);
        return;
    }

    if (do_list) {
        prv_ksh_devmgr_list();
    } else if (do_list_pci) {
        prv_ksh_devmgr_list_pci();
    } else if (do_dump_pci) {
        prv_ksh_devmgr_dump_pci(pci_id_str);
    }

    ksharg_free_parser_inst(parser);
}

static void prv_ksh_devmgr_list(void) {
    devmgr_iter_t dev_iter;
    devmgr_iter_init(&dev_iter, DEVMGR_CLASS_NONE);

    size_t num_devs = 0;
    devmgr_dev_t *dev;
    while ((dev = devmgr_iter_next(&dev_iter))) {
        kprintf("id %u, class '%s', driver '%s'\n", dev->id,
                devmgr_class_name(dev->dev_class),
                devmgr_driver_name(dev->driver_id));
        num_devs++;
    }

    kprintf("%u device(s)\n", num_devs);
}

static void prv_ksh_devmgr_list_pci(void) {
    const size_t num_pci_devs = pci_num_devs();
    for (size_t dev_idx = 0; dev_idx < num_pci_devs; dev_idx++) {
        const pci_dev_t *const pci_dev = pci_get_dev_const(dev_idx);
        pci_dump_dev_short(pci_dev);
    }
    kprintf("%u PCI device(s)\n", num_pci_devs);
}

static void prv_ksh_devmgr_dump_pci(const char *id_str) {
    uint32_t id;
    if (!string_to_uint32(id_str, &id, 10)) {
        kprintf("ksh_devmgr: bad integer '%u'\n", id_str);
        return;
    }

    const pci_dev_t *const pci_dev = pci_get_dev_const(id);
    if (pci_dev) {
        pci_dump_dev_header(pci_dev);
    } else {
        kprintf("ksh_devmgr: no PCI device with ID %u\n", id);
    }
}
