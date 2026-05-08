#include "assert.h"
#include "fs/ramfs.h"
#include "heap.h"
#include "kstring.h"
#include "log.h"
#include "memfun.h"
#include "vfs/vnode.h"

typedef enum {
    RAMFS_DIR,
    RAMFS_FILE,
} ramfs_node_type_t;

struct ramfs_node {
    vnode_t *vnode;
    ramfs_node_t *parent_node;

    ramfs_node_type_t type;
    union {
        struct {
            void *buf;
            size_t buf_size;
        } file_data;
        struct {
            ramfs_node_t **children;
            dirent_t *dirents;
            size_t num_children;
        } dir_data;
    };
};

static const fs_desc_t g_ramfs_desc = {
    .name = "ramfs",
    .f_mount = ramfs_mount,
};
static const vnode_ops_t g_ramfs_node_ops = {
    .f_mknode = ramfs_node_mknode,
    .f_readdir = ramfs_node_readdir,
    .f_lookup = ramfs_node_lookup,
    .f_read = ramfs_node_read,
};

static ramfs_node_t *prv_ramfs_alloc_node(ramfs_ctx_t *ctx,
                                          ramfs_node_type_t type);
static void prv_ramfs_free_node(ramfs_ctx_t *ctx, ramfs_node_t *fsnode);

static bool prv_ramfs_find_child(ramfs_node_t *dir_fsnode, const char *name,
                                 ramfs_node_t **child_fsnode);
static vfs_err_t prv_ramfs_make_dir_node(ramfs_ctx_t *ctx,
                                         ramfs_node_t *parent_fsnode,
                                         ramfs_node_t **out_dir_fsnode);
static vfs_err_t prv_ramfs_add_child(ramfs_ctx_t *ctx, ramfs_node_t *dir_fsnode,
                                     const char *child_name,
                                     ramfs_node_t *child_fsnode);

ramfs_ctx_t *ramfs_init(size_t num_bytes) {
    if (num_bytes < sizeof(ramfs_ctx_t)) { return NULL; }

    ramfs_ctx_t *const ctx = heap_alloc(sizeof(*ctx));
    kmemset(ctx, 0, sizeof(*ctx));

    ctx->bytes_used = sizeof(ramfs_ctx_t);
    ctx->size = num_bytes;

    vfs_err_t err = prv_ramfs_make_dir_node(ctx, NULL, &ctx->root);
    if (err != VFS_ERR_NONE) {
        LOG_ERROR("failed to create root dir node: %u", err);
        return NULL;
    }

    return ctx;
}

void ramfs_free(ramfs_ctx_t *ctx) {
    // TODO: iterate through the tree and free every node node.
    heap_free(ctx);
}

const fs_desc_t *ramfs_get_desc(void) {
    return &g_ramfs_desc;
}

vfs_err_t ramfs_mount(void *v_ctx, vnode_t *node) {
    ramfs_ctx_t *const ctx = v_ctx;

    if (ctx->root->vnode) { return VFS_ERR_FS_ALREADY_MOUNTED; }
    if (node->fs_ctx) { return VFS_ERR_NODE_ALREADY_MOUNTED; }
    if (node->type != VNODE_DIR) { return VFS_ERR_NODE_NOT_DIR; }

    node->flags |= VNODE_ROOT;
    node->ops = &g_ramfs_node_ops;
    node->fs_ctx = ctx;
    node->fs_data = ctx->root;

    ctx->root->vnode = node;

    return VFS_ERR_NONE;
}

vfs_err_t ramfs_unmount(void *v_ctx, vnode_t *node) {
    ramfs_ctx_t *const ctx = v_ctx;

    if (node->fs_ctx != ctx) { return VFS_ERR_NODE_NOT_MOUNTED; }

    node->flags &= ~VNODE_ROOT;
    node->ops = NULL;
    node->fs_ctx = NULL;
    node->fs_data = NULL;

    ctx->root->vnode = NULL;

    return VFS_ERR_NONE;
}

vfs_err_t ramfs_node_mknode(vnode_t *dir_node, vnode_t **out_node,
                            const char *name, vnode_type_t node_type) {
    if (!dir_node) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!out_node) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!name) { return VFS_ERR_NODE_BAD_ARGS; }
    if (string_len(name) + 1 > VNODE_MAX_NAME_SIZE) {
        return VFS_ERR_NODE_NAME_TOO_LONG;
    }

    ramfs_ctx_t *const ctx = dir_node->fs_ctx;
    ramfs_node_t *const fsnode = dir_node->fs_data;
    if (!ctx) { return VFS_ERR_NODE_NO_FS; }
    if (!fsnode) { return VFS_ERR_NODE_NO_DATA; }

    if (prv_ramfs_find_child(fsnode, name, NULL)) { return VFS_ERR_NAME_TAKEN; }

    vfs_err_t err;
    ramfs_node_t *new_fsnode = NULL;
    if (node_type == VNODE_FILE) {
        new_fsnode = prv_ramfs_alloc_node(ctx, RAMFS_FILE);
        if (!new_fsnode) { return VFS_ERR_FS_NO_SPACE; }

        // Prefill files with data.
        size_t prefill_size = 32;
        if (ctx->bytes_used + prefill_size > ctx->size) {
            return VFS_ERR_FS_NO_SPACE;
        }
        new_fsnode->file_data.buf = heap_alloc(prefill_size);
        new_fsnode->file_data.buf_size = prefill_size;
        for (size_t idx = 0; idx < prefill_size; idx++) {
            uint8_t *ptr = &((uint8_t *)new_fsnode->file_data.buf)[idx];
            *ptr = 2 * idx;
        }
    } else if (node_type == VNODE_DIR) {
        err = prv_ramfs_make_dir_node(ctx, fsnode, &new_fsnode);
        if (err != VFS_ERR_NONE) { return err; }
    } else {
        return VFS_ERR_NODE_BAD_ARGS;
    }

    vnode_t *const new_node = vnode_get();
    new_node->type = node_type;
    new_node->flags = 0;
    new_node->ops = &g_ramfs_node_ops;
    new_node->size =
        node_type == VNODE_FILE ? new_fsnode->file_data.buf_size : 0;
    new_node->fs_ctx = ctx;
    new_node->fs_data = new_fsnode;

    new_fsnode->parent_node = fsnode;
    new_fsnode->vnode = new_node;

    err = prv_ramfs_add_child(ctx, fsnode, name, new_fsnode);
    if (err != VFS_ERR_NONE) {
        vnode_put(new_node);
        prv_ramfs_free_node(ctx, new_fsnode);
        return err;
    }

    *out_node = new_node;

    return VFS_ERR_NONE;
}

vfs_err_t ramfs_node_readdir(vnode_t *node, void *dirent_buf, size_t buf_len,
                             size_t *out_len) {
    if (!node) { return VFS_ERR_NODE_BAD_ARGS; }
    if (node->type != VNODE_DIR) { return VFS_ERR_NODE_NOT_DIR; }
    if (!dirent_buf) { return VFS_ERR_NODE_BAD_ARGS; }
    if (buf_len == 0) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!out_len) { return VFS_ERR_NODE_BAD_ARGS; }

    ramfs_ctx_t *const ctx = node->fs_ctx;
    ramfs_node_t *const fsnode = node->fs_data;
    if (!ctx) { return VFS_ERR_NODE_NO_FS; }
    if (!fsnode) { return VFS_ERR_NODE_NO_DATA; }

    const size_t copy_len = fsnode->dir_data.num_children <= buf_len
                                ? fsnode->dir_data.num_children
                                : buf_len;
    kmemcpy(dirent_buf, fsnode->dir_data.dirents, copy_len * sizeof(dirent_t));
    *out_len = copy_len;

    return VFS_ERR_NONE;
}

vfs_err_t ramfs_node_lookup(vnode_t *node, vnode_t **out_node,
                            const char *name) {
    if (!node) { return VFS_ERR_NODE_BAD_ARGS; }
    if (node->type != VNODE_DIR) { return VFS_ERR_NODE_NOT_DIR; }
    if (!out_node) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!name) { return VFS_ERR_NODE_BAD_ARGS; }

    ramfs_ctx_t *const ctx = node->fs_ctx;
    ramfs_node_t *const data = node->fs_data;
    if (!ctx) { return VFS_ERR_NODE_NO_FS; }
    if (!data) { return VFS_ERR_NODE_NO_DATA; }

    ramfs_node_t *child_fsnode;
    if (!prv_ramfs_find_child(data, name, &child_fsnode)) {
        return VFS_ERR_NODE_NOT_FOUND;
    }

    if (!child_fsnode->vnode) {
        vnode_t *const vnode = vnode_get();
        vnode->flags = 0;
        vnode->ops = &g_ramfs_node_ops;
        vnode->fs_ctx = node->fs_ctx;
        vnode->fs_data = child_fsnode;

        switch (child_fsnode->type) {
        case RAMFS_DIR:
            vnode->type = VNODE_DIR;
            break;
        case RAMFS_FILE:
            vnode->type = VNODE_FILE;
            break;
        }

        child_fsnode->vnode = vnode;
    }

    *out_node = child_fsnode->vnode;
    return VFS_ERR_NONE;
}

vfs_err_t ramfs_node_read(vnode_t *node, size_t offset, void *buf,
                          size_t num_bytes, size_t *out_read) {
    if (!node) { return VFS_ERR_NODE_BAD_ARGS; }
    if (node->type != VNODE_FILE) { return VFS_ERR_NODE_NOT_DIR; }
    if (offset >= node->size) { return VFS_ERR_NODE_BAD_OFFSET; }
    if (!buf) { return VFS_ERR_NODE_BAD_ARGS; }

    ramfs_ctx_t *const ctx = node->fs_ctx;
    ramfs_node_t *const fsnode = node->fs_data;
    if (!ctx) { return VFS_ERR_NODE_NO_FS; }
    if (!fsnode) { return VFS_ERR_NODE_NO_DATA; }

    ASSERT(fsnode->type == RAMFS_FILE);
    DEBUG_ASSERT(node->size == fsnode->file_data.buf_size);

    // FIXME: check for overflow
    if (offset + num_bytes >= node->size) { num_bytes = node->size - offset; }

    kmemcpy(buf, (void *)((uintptr_t)fsnode->file_data.buf + offset),
            num_bytes);
    *out_read = num_bytes;

    return VFS_ERR_NONE;
}

static ramfs_node_t *prv_ramfs_alloc_node(ramfs_ctx_t *ctx,
                                          ramfs_node_type_t type) {
    const size_t req_size = sizeof(ramfs_node_t);
    if (ctx->bytes_used + req_size > ctx->size) {
        // out of space
        return NULL;
    }

    ramfs_node_t *const fsnode = heap_alloc(sizeof(*fsnode));
    kmemset(fsnode, 0, sizeof(*fsnode));
    fsnode->type = type;

    ctx->bytes_used += req_size;

    return fsnode;
}

static void prv_ramfs_free_node(ramfs_ctx_t *ctx, ramfs_node_t *fsnode) {
    switch (fsnode->type) {
    case RAMFS_DIR:
        if (fsnode->dir_data.children) { heap_free(fsnode->dir_data.children); }
        if (fsnode->dir_data.dirents) { heap_free(fsnode->dir_data.children); }
        ctx->bytes_used -=
            sizeof(ramfs_node_t) +
            fsnode->dir_data.num_children *
                sizeof(fsnode->dir_data.children[0]) +
            fsnode->dir_data.num_children * sizeof(fsnode->dir_data.dirents[0]);
        break;
    case RAMFS_FILE:
        if (fsnode->file_data.buf) { heap_free(fsnode->file_data.buf); }
        ctx->bytes_used -= sizeof(ramfs_node_t) + fsnode->file_data.buf_size;
        break;
    }

    heap_free(fsnode);
}

static bool prv_ramfs_find_child(ramfs_node_t *dir_node, const char *name,
                                 ramfs_node_t **child_node) {
    for (size_t idx = 0; idx < dir_node->dir_data.num_children; idx++) {
        const dirent_t *const dirent = &dir_node->dir_data.dirents[idx];
        if (string_equals(name, dirent->name)) {
            if (child_node) { *child_node = dir_node->dir_data.children[idx]; }
            return true;
        }
    }
    return false;
}

static vfs_err_t prv_ramfs_make_dir_node(ramfs_ctx_t *ctx,
                                         ramfs_node_t *parent_node,
                                         ramfs_node_t **out_dir_node) {
    if (parent_node) { ASSERT(parent_node->type == RAMFS_DIR); }

    ramfs_node_t *const dir_fsnode = prv_ramfs_alloc_node(ctx, RAMFS_DIR);
    if (!dir_fsnode) { return VFS_ERR_FS_NO_SPACE; }

    if (!parent_node) { parent_node = dir_fsnode; }
    dir_fsnode->parent_node = parent_node;

    const size_t num_children = 2; // '.' and '..'
    const size_t child_size = sizeof(ramfs_node_t *) + sizeof(dirent_t);
    const size_t req_size = num_children * child_size;
    if (ctx->bytes_used + req_size > ctx->size) {
        prv_ramfs_free_node(ctx, dir_fsnode);
        return VFS_ERR_FS_NO_SPACE;
    }

    dir_fsnode->dir_data.num_children = num_children;
    dir_fsnode->dir_data.dirents = heap_alloc(num_children * sizeof(dirent_t));
    dir_fsnode->dir_data.children =
        heap_alloc(num_children * sizeof(ramfs_node_t *));

    const char *const name_curr_dir = ".";
    const char *const name_parent_dir = "..";
    kmemcpy(dir_fsnode->dir_data.dirents[0].name, name_curr_dir, 2);
    kmemcpy(dir_fsnode->dir_data.dirents[1].name, name_parent_dir, 3);

    dir_fsnode->dir_data.children[0] = dir_fsnode;
    dir_fsnode->dir_data.children[1] = parent_node;

    *out_dir_node = dir_fsnode;
    return VFS_ERR_NONE;
}

static vfs_err_t prv_ramfs_add_child(ramfs_ctx_t *ctx, ramfs_node_t *dir_fsnode,
                                     const char *child_name,
                                     ramfs_node_t *child_fsnode) {
    ASSERT(dir_fsnode->type == RAMFS_DIR);

    const size_t new_idx = dir_fsnode->dir_data.num_children;
    const size_t new_len = new_idx + 1;

    const size_t req_size = sizeof(ramfs_node_t *) + sizeof(dirent_t);
    if (ctx->bytes_used + req_size > ctx->size) { return VFS_ERR_FS_NO_SPACE; }
    ctx->bytes_used += req_size;

    if (dir_fsnode->dir_data.dirents) {
        dir_fsnode->dir_data.dirents = heap_realloc(
            dir_fsnode->dir_data.dirents, new_len * sizeof(dirent_t), 4);
    } else {
        dir_fsnode->dir_data.dirents = heap_alloc(new_len * sizeof(dirent_t));
    }

    if (dir_fsnode->dir_data.children) {
        dir_fsnode->dir_data.children =
            heap_realloc(dir_fsnode->dir_data.children,
                         new_len * sizeof(dir_fsnode->dir_data.children[0]), 4);
    } else {
        dir_fsnode->dir_data.children =
            heap_alloc(new_len * sizeof(ramfs_node_t *));
    }

    dir_fsnode->dir_data.children[new_idx] = child_fsnode;
    kmemcpy(dir_fsnode->dir_data.dirents[new_idx].name, child_name,
            string_len(child_name) + 1);
    dir_fsnode->dir_data.num_children = new_len;

    return VFS_ERR_NONE;
}
