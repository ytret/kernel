#include "heap.h"
#include "kinttypes.h"
#include "kprintf.h"
#include "kshell/ksharg.h"
#include "kshell/kshcmd/ksh_vfs.h"
#include "kstring.h"
#include "vfs/vnode.h"

#define KSH_VFS_LS_MAX_DIRENTS 64

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
static void prv_ksh_vfs_mkdir(const char *path_str);
static void prv_ksh_vfs_mkfile(const char *path_str);
static void prv_ksh_vfs_unlink(const char *path_str);
static void prv_ksh_vfs_rmdir(const char *path_str);

static bool prv_ksh_vfs_resolve_path(const char *path, vnode_t **out_node);
static bool prv_ksh_vfs_get_parent_node(const char *path_str,
                                        vnode_t **out_node,
                                        char **out_basename);

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
    } else if (string_equals(action_str, "mkdir")) {
        prv_ksh_vfs_mkdir(path_str);
    } else if (string_equals(action_str, "mkfile")) {
        prv_ksh_vfs_mkfile(path_str);
    } else if (string_equals(action_str, "unlink")) {
        prv_ksh_vfs_unlink(path_str);
    } else if (string_equals(action_str, "rmdir")) {
        prv_ksh_vfs_rmdir(path_str);
    } else {
        kprintf("ksh_vfs: unrecognized action '%s'\n", action_str);
    }

    ksharg_free_parser_inst(parser);
}

static void prv_ksh_vfs_ls(const char *path) {
    vnode_t *node;
    if (!prv_ksh_vfs_resolve_path(path, &node)) { return; }

    if (!node->ops) {
        kprintf("ksh_vfs: node at path '%s' has no ops\n", path);
        return;
    }
    if (!node->ops->f_readdir) {
        kprintf("ksh_vfs: node at path '%s' does not support op 'readdir'\n",
                path);
        return;
    }

    constexpr size_t max_dirents = KSH_VFS_LS_MAX_DIRENTS;
    dirent_t *const dirents = heap_alloc(max_dirents * sizeof(dirent_t));

    mutex_acquire(&node->lock);
    size_t read_dirents;
    auto f_readdir = node->ops->f_readdir;
    kerr_t err = f_readdir(node, dirents, max_dirents, &read_dirents);
    mutex_release(&node->lock);

    if (err != KERR_NONE) {
        kprintf("ksh_vfs: op 'readdir' returned error code %u: %s\n", err,
                kerr_str(err));
        return;
    }

    for (size_t idx = 0; idx < read_dirents; idx++) {
        const dirent_t *const dirent = &dirents[idx];
        kprintf("%s\n", dirent->name);
    }

    if (read_dirents == max_dirents) {
        kprintf("ksh_vfs: reached the maximum number of dirents (%" PRIu32
                "), buffer "
                "length needs to be increased\n",
                max_dirents);
    }
}

static void prv_ksh_vfs_mkdir(const char *path_str) {
    kerr_t err;
    vnode_t *parent_node;
    char *basename;

    if (!prv_ksh_vfs_get_parent_node(path_str, &parent_node, &basename)) {
        // ^ should have printed the error message.
        return;
    }

    if (!parent_node->ops) {
        kprintf("ksh_vfs: node at path '%s' has no ops\n", path_str);
        heap_free(basename);
        return;
    }
    if (!parent_node->ops->f_mknode) {
        kprintf("ksh_vfs: node at path '%s' does not support op 'mknode'\n",
                path_str);
        heap_free(basename);
        return;
    }

    mutex_acquire(&parent_node->lock);
    vnode_t *child_node;
    auto f_mknode = parent_node->ops->f_mknode;
    err = f_mknode(parent_node, &child_node, basename, VNODE_DIR);
    mutex_release(&parent_node->lock);

    if (err != KERR_NONE) {
        kprintf("ksh_vfs: op 'mknode' returned error code %u: %s\n", err,
                kerr_str(err));
        heap_free(basename);
        return;
    }

    kprintf("ksh_vfs: created directory node at '%s'\n", path_str);
    heap_free(basename);
}

static void prv_ksh_vfs_mkfile(const char *path_str) {
    kerr_t err;
    vnode_t *parent_node;
    char *basename;

    if (!prv_ksh_vfs_get_parent_node(path_str, &parent_node, &basename)) {
        // ^ should have printed the error message.
        return;
    }

    if (!parent_node->ops) {
        kprintf("ksh_vfs: node at path '%s' has no ops\n", path_str);
        heap_free(basename);
        return;
    }
    if (!parent_node->ops->f_mknode) {
        kprintf("ksh_vfs: node at path '%s' does not support op 'mknode'\n",
                path_str);
        heap_free(basename);
        return;
    }

    mutex_acquire(&parent_node->lock);
    vnode_t *child_node;
    auto f_mknode = parent_node->ops->f_mknode;
    err = f_mknode(parent_node, &child_node, basename, VNODE_FILE);
    mutex_release(&parent_node->lock);

    if (err != KERR_NONE) {
        kprintf("ksh_vfs: op 'mknode' returned error code %u: %s\n", err,
                kerr_str(err));
        heap_free(basename);
        return;
    }

    kprintf("ksh_vfs: created file node at '%s'\n", path_str);
    heap_free(basename);
}

static void prv_ksh_vfs_unlink(const char *path_str) {
    kerr_t err;
    vnode_t *parent_node;
    char *basename;

    if (!prv_ksh_vfs_get_parent_node(path_str, &parent_node, &basename)) {
        // ^ should have printed the error message.
        return;
    }

    if (!parent_node->ops) {
        kprintf("ksh_vfs: node at path '%s' has no ops\n", path_str);
        heap_free(basename);
        return;
    }
    if (!parent_node->ops->f_unlink) {
        kprintf("ksh_vfs: node at path '%s' does not support op 'unlink'\n",
                path_str);
        heap_free(basename);
        return;
    }

    mutex_acquire(&parent_node->lock);
    auto f_unlink = parent_node->ops->f_unlink;
    err = f_unlink(parent_node, basename);
    mutex_release(&parent_node->lock);

    if (err != KERR_NONE) {
        kprintf("ksh_vfs: op 'unlink' returned error code %u: %s\n", err,
                kerr_str(err));
        heap_free(basename);
        return;
    }

    kprintf("ksh_vfs: removed file node at '%s'\n", path_str);
    heap_free(basename);
}

static void prv_ksh_vfs_rmdir(const char *path_str) {
    kerr_t err;
    vnode_t *parent_node;
    char *basename;

    if (!prv_ksh_vfs_get_parent_node(path_str, &parent_node, &basename)) {
        // ^ should have printed the error message.
        return;
    }

    if (!parent_node->ops) {
        kprintf("ksh_vfs: node at path '%s' has no ops\n", path_str);
        heap_free(basename);
        return;
    }
    if (!parent_node->ops->f_rmdir) {
        kprintf("ksh_vfs: node at path '%s' does not support op 'rmdir'\n",
                path_str);
        heap_free(basename);
        return;
    }

    mutex_acquire(&parent_node->lock);
    auto f_rmdir = parent_node->ops->f_rmdir;
    err = f_rmdir(parent_node, basename);
    mutex_release(&parent_node->lock);

    if (err != KERR_NONE) {
        kprintf("ksh_vfs: op 'rmdir' returned error code %u: %s\n", err,
                kerr_str(err));
        heap_free(basename);
        return;
    }

    kprintf("ksh_vfs: removed dir node at '%s'\n", path_str);
    heap_free(basename);
}

static bool prv_ksh_vfs_resolve_path(const char *path, vnode_t **out_node) {
    kerr_t err = vnode_resolve_path_str(path, out_node);
    if (err == KERR_NONE) {
        return true;
    } else {
        kprintf("ksh_vfs: failed to resolve path '%s' with error code %u: %s\n",
                path, err, kerr_str(err));
        return false;
    }
}

static bool prv_ksh_vfs_get_parent_node(const char *path_str,
                                        vnode_t **out_node,
                                        char **out_basename) {
    vpath_t path;
    kerr_t path_err = vpath_from_str(path_str, &path);
    if (path_err != KERR_NONE) {
        kprintf(
            "ksh_vfs: failed to convert '%s' to a path object, error %u: %s\n",
            path_str, path_err, kerr_str(path_err));
        vpath_free(&path);
        return false;
    }

    if (list_is_empty(&path.parts)) {
        kprintf("ksh_vfs: path '%s' has too few parts\n", path_str);
        return false;
    }
    const list_node_t *const basename_lnode = list_pop_last(&path.parts);
    const vpath_part_t *const path_last_part =
        LIST_NODE_TO_STRUCT(basename_lnode, vpath_part_t, list_node);
    const char *const basename = path_last_part->name;

    vnode_t *parent_node = NULL;
    kerr_t err = vnode_resolve_path(&path, &parent_node);
    if (err != KERR_NONE) {
        kprintf("ksh_vfs: failed to resolve '%s' without its last part, error "
                "%u: %s\n",
                path_str, err, kerr_str(err));
        if (parent_node) { vnode_put(parent_node); }
        vpath_free(&path);
        return false;
    }

    if (out_node) { *out_node = parent_node; }
    if (out_basename) { *out_basename = string_dup(basename); }

    vpath_free(&path);
    return true;
}
