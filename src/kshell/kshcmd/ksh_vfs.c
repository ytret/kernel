#include "heap.h"
#include "kprintf.h"
#include "kshell/ksharg.h"
#include "kshell/kshcmd/ksh_vfs.h"
#include "kstring.h"
#include "vfs/vfs.h"

static ksharg_posarg_desc_t g_ksh_vfs_posargs[] = {
    {
        .name = "action",
        .help_str = "Action to perform (one of: help, ls, mkdir, mkfile).",
        .def_val_str = "help",
    },
    {
        .name = "path",
        .help_str = "Absolute node path to perform the action on.",
        .def_val_str = "/",
    },
};

static ksharg_flag_desc_t g_ksh_vfs_flags[] = {
    {
        .short_name = "h",
        .long_name = "help",
        .help_str = "Print this message and exit.",
        .val_name = NULL,
    },
};

static const ksharg_parser_desc_t g_ksh_vfs_parser = {
    .name = "vfs",
    .description = "Virtual file system interaction.",
    .epilog = NULL,

    .num_posargs = sizeof(g_ksh_vfs_posargs) / sizeof(g_ksh_vfs_posargs[0]),
    .posargs = g_ksh_vfs_posargs,

    .num_flags = sizeof(g_ksh_vfs_flags) / sizeof(g_ksh_vfs_flags[0]),
    .flags = g_ksh_vfs_flags,
};

static void prv_ksh_vfs_ls(const char *path);
static bool prv_ksh_vfs_resolve_path(const char *path, vfs_node_t **out_node);

void ksh_vfs(list_t *arg_list) {
    ksharg_parser_inst_t *parser;
    ksharg_err_t err;

    err = ksharg_inst_parser(&g_ksh_vfs_parser, &parser);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_vfs: error instantiating the argument parser: %u\n", err);
        return;
    }

    err = ksharg_parse_list(parser, arg_list);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_vfs: error parsing arguments: %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }

    bool do_help;
    const char *action_str;
    const char *path_str;

    ksharg_flag_inst_t *flag_help;
    err = ksharg_get_flag_inst(parser, "help", &flag_help);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_vfs: error getting flag 'help': %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }
    do_help = flag_help->given_str;

    ksharg_posarg_inst_t *posarg_action;
    err = ksharg_get_posarg_inst(parser, "action", &posarg_action);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_vfs: error getting positional argument 'action': %u\n",
                err);
        ksharg_free_parser_inst(parser);
        return;
    }
    action_str = posarg_action->given_str;

    ksharg_posarg_inst_t *posarg_path;
    err = ksharg_get_posarg_inst(parser, "path", &posarg_path);
    if (err != KSHARG_ERR_NONE) {
        kprintf("ksh_vfs: error getting positional argument 'path': %u\n", err);
        ksharg_free_parser_inst(parser);
        return;
    }
    path_str = posarg_path->given_str;

    if (do_help || string_equals(action_str, "help")) {
        ksharg_print_help(&g_ksh_vfs_parser);
        ksharg_free_parser_inst(parser);
        return;
    }

    if (string_equals(action_str, "ls")) {
        prv_ksh_vfs_ls(path_str);
    } else {
        kprintf("ksh_vfs: unrecognized action '%s'\n", action_str);
    }

    ksharg_free_parser_inst(parser);
}

static void prv_ksh_vfs_ls(const char *path) {
    vfs_node_t *node;
    if (!prv_ksh_vfs_resolve_path(path, &node)) { return; }

    if (!node->ops) {
        kprintf("ksh_vfs: node at path '%s' has no ops\n", path);
        return;
    }
    if (!node->ops->f_readdir) {
        kprintf(
            "ksh_vfs: node at path '%s' does not support action 'readdir'\n",
            path);
        return;
    }

    constexpr size_t max_dirents = 10;
    vfs_dirent_t *const dirents =
        heap_alloc(max_dirents * sizeof(vfs_dirent_t));

    size_t read_dirents;
    auto f_readdir = node->ops->f_readdir;
    vfs_err_t err = f_readdir(node, dirents, max_dirents, &read_dirents);

    if (err != VFS_ERR_NONE) {
        kprintf("ksh_vfs: op readdir returned error code %u: %s\n", err,
                vfs_err_str(err));
        return;
    }

    for (size_t idx = 0; idx < read_dirents; idx++) {
        const vfs_dirent_t *const dirent = &dirents[idx];
        kprintf("%s\n", dirent->name);
    }

    if (read_dirents == max_dirents) {
        kprintf("ksh_vfs: reached the maximum number of dirents (%u), buffer "
                "length needs to be increased\n",
                max_dirents);
    }
}

static bool prv_ksh_vfs_resolve_path(const char *path, vfs_node_t **out_node) {
    vfs_err_t err = vfs_resolve_path_str(path, out_node);
    if (err == VFS_ERR_NONE) {
        return true;
    } else {
        kprintf("ksh_vfs: failed to resolve path '%s' with error code %u: %s\n",
                path, err, vfs_err_str(err));
        return false;
    }
}
