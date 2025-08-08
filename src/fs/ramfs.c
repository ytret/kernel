#include "assert.h"
#include "fs/ramfs.h"
#include "heap.h"
#include "kstring.h"
#include "memfun.h"
#include "vfs/vfs.h"

typedef enum {
    RAMFS_DATA_DIR,
    RAMFS_DATA_FILE,
} ramfs_data_type_t;

struct ramfs_data {
    vfs_node_t *vfs_node;
    ramfs_data_t *parent_data;

    ramfs_data_type_t type;
    union {
        struct {
            void *buf;
            size_t buf_size;
        } file_data;
        struct {
            ramfs_data_t **children;
            vfs_dirent_t *dirents;
            size_t num_children;
        } dir_data;
    };
};

static const vfs_fs_desc_t g_ramfs_desc = {
    .name = "ramfs",
    .f_mount = ramfs_mount,
};
static const vfs_node_ops_t g_ramfs_node_ops = {
    .f_mknode = ramfs_node_mknode,
    .f_readdir = ramfs_node_readdir,
};

static ramfs_data_t *prv_ramfs_alloc_data(ramfs_ctx_t *ctx,
                                          ramfs_data_type_t type);
static void prv_ramfs_free_data(ramfs_ctx_t *ctx, ramfs_data_t *data);

static bool prv_ramfs_find_child(ramfs_data_t *dir_data, const char *name,
                                 ramfs_data_t **child_data);
static vfs_err_t prv_ramfs_add_child(ramfs_ctx_t *ctx, ramfs_data_t *dir_data,
                                     const char *name, ramfs_data_type_t type);

ramfs_ctx_t *ramfs_init(size_t num_bytes) {
    if (num_bytes < sizeof(ramfs_ctx_t)) { return NULL; }

    ramfs_ctx_t *const ctx = heap_alloc(sizeof(*ctx));
    kmemset(ctx, 0, sizeof(*ctx));

    ctx->bytes_used = sizeof(ramfs_ctx_t);
    ctx->size = num_bytes;

    ctx->root = prv_ramfs_alloc_data(ctx, RAMFS_DATA_DIR);
    if (ctx->root) {
        ctx->root->parent_data = ctx->root;
        return ctx;
    } else {
        heap_free(ctx);
        return NULL;
    }
}

void ramfs_free(ramfs_ctx_t *ctx) {
    // TODO: iterate through the tree and free every node data.
    heap_free(ctx);
}

const vfs_fs_desc_t *ramfs_get_desc(void) {
    return &g_ramfs_desc;
}

vfs_err_t ramfs_mount(void *v_ctx, vfs_node_t *node) {
    ramfs_ctx_t *const ctx = v_ctx;

    if (ctx->root->vfs_node) { return VFS_ERR_FS_ALREADY_MOUNTED; }
    if (node->fs_ctx) { return VFS_ERR_NODE_ALREADY_MOUNTED; }
    if (node->type != VFS_NODE_DIR) { return VFS_ERR_NODE_NOT_DIR; }

    node->flags |= VFS_NODE_ROOT;
    node->ops = &g_ramfs_node_ops;
    node->fs_ctx = ctx;
    node->fs_data = ctx->root;

    ctx->root->vfs_node = node;

    return VFS_ERR_NONE;
}

vfs_err_t ramfs_unmount(void *v_ctx, vfs_node_t *node) {
    ramfs_ctx_t *const ctx = v_ctx;

    if (node->fs_ctx != ctx) { return VFS_ERR_NODE_NOT_MOUNTED; }

    node->flags &= ~VFS_NODE_ROOT;
    node->ops = NULL;
    node->fs_ctx = NULL;
    node->fs_data = NULL;

    ctx->root->vfs_node = NULL;

    return VFS_ERR_NONE;
}

vfs_err_t ramfs_node_mknode(vfs_node_t *dir_node, vfs_node_t **out_node,
                            const char *name, vfs_node_type_t node_type) {
    if (!dir_node) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!out_node) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!name) { return VFS_ERR_NODE_BAD_ARGS; }
    if (string_len(name) + 1 > VFS_NODE_MAX_NAME_SIZE) {
        return VFS_ERR_NODE_NAME_TOO_LONG;
    }

    ramfs_ctx_t *const ctx = dir_node->fs_ctx;
    ramfs_data_t *const data = dir_node->fs_data;
    if (!ctx) { return VFS_ERR_NODE_NO_FS; }
    if (!data) { return VFS_ERR_NODE_NO_DATA; }

    if (prv_ramfs_find_child(data, name, NULL)) { return VFS_ERR_NAME_TAKEN; }

    ramfs_data_type_t data_type;
    switch (node_type) {
    case VFS_NODE_DIR:
        data_type = RAMFS_DATA_DIR;
        break;
    case VFS_NODE_FILE:
        data_type = RAMFS_DATA_FILE;
        break;
    default:
        return VFS_ERR_NODE_BAD_ARGS;
    }
    ramfs_data_t *const new_data = prv_ramfs_alloc_data(ctx, data_type);
    if (!new_data) { return VFS_ERR_FS_NO_SPACE; }

    vfs_node_t *const new_node = vfs_alloc_node();
    new_node->type = node_type;
    new_node->flags = 0;
    new_node->ops = &g_ramfs_node_ops;
    new_node->fs_ctx = ctx;
    new_node->fs_data = new_data;

    new_data->parent_data = data;
    new_data->vfs_node = new_node;

    vfs_err_t err = prv_ramfs_add_child(ctx, data, name, data_type);
    if (err != VFS_ERR_NONE) {
        vfs_free_node(new_node);
        prv_ramfs_free_data(ctx, new_data);
        return err;
    }

    *out_node = new_node;

    return VFS_ERR_NONE;
}

vfs_err_t ramfs_node_readdir(vfs_node_t *node, void *dirent_buf, size_t buf_len,
                             size_t *out_len) {
    if (!node) { return VFS_ERR_NODE_BAD_ARGS; }
    if (node->type != VFS_NODE_DIR) { return VFS_ERR_NODE_NOT_DIR; }
    if (!dirent_buf) { return VFS_ERR_NODE_BAD_ARGS; }
    if (buf_len == 0) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!out_len) { return VFS_ERR_NODE_BAD_ARGS; }

    ramfs_ctx_t *const ctx = node->fs_ctx;
    ramfs_data_t *const data = node->fs_data;
    if (!ctx) { return VFS_ERR_NODE_NO_FS; }
    if (!data) { return VFS_ERR_NODE_NO_DATA; }

    const size_t copy_len = data->dir_data.num_children <= buf_len
                                ? data->dir_data.num_children
                                : buf_len;
    kmemcpy(dirent_buf, data->dir_data.dirents,
            copy_len * sizeof(vfs_dirent_t));
    *out_len = copy_len;

    return VFS_ERR_NONE;
}

vfs_err_t ramfs_node_lookup(vfs_node_t *node, vfs_node_t **out_node,
                            const char *name) {
    if (!node) { return VFS_ERR_NODE_BAD_ARGS; }
    if (node->type != VFS_NODE_DIR) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!out_node) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!name) { return VFS_ERR_NODE_BAD_ARGS; }

    ramfs_ctx_t *const ctx = node->fs_ctx;
    ramfs_data_t *const data = node->fs_data;
    if (!ctx) { return VFS_ERR_NODE_NO_FS; }
    if (!data) { return VFS_ERR_NODE_NO_DATA; }

    ramfs_data_t *child_data;
    if (!prv_ramfs_find_child(data, name, &child_data)) {
        return VFS_ERR_NODE_NOT_FOUND;
    }

    if (!child_data->vfs_node) {
        vfs_node_t *const vfs_node = vfs_alloc_node();
        vfs_node->flags = 0;
        vfs_node->ops = &g_ramfs_node_ops;
        vfs_node->fs_ctx = node->fs_ctx;
        vfs_node->fs_data = child_data;

        switch (child_data->type) {
        case RAMFS_DATA_DIR:
            vfs_node->type = VFS_NODE_DIR;
            break;
        case RAMFS_DATA_FILE:
            vfs_node->type = VFS_NODE_FILE;
            break;
        }

        child_data->vfs_node = vfs_node;
    }

    *out_node = child_data->vfs_node;
    return VFS_ERR_NONE;
}

static ramfs_data_t *prv_ramfs_alloc_data(ramfs_ctx_t *ctx,
                                          ramfs_data_type_t type) {
    const size_t req_size = sizeof(ramfs_data_t);
    if (ctx->bytes_used + req_size > ctx->size) {
        // out of space
        return NULL;
    }

    ramfs_data_t *const data = heap_alloc(sizeof(*data));
    kmemset(data, 0, sizeof(*data));
    data->type = type;

    ctx->bytes_used += req_size;

    return data;
}

static void prv_ramfs_free_data(ramfs_ctx_t *ctx, ramfs_data_t *data) {
    switch (data->type) {
    case RAMFS_DATA_DIR:
        if (data->dir_data.children) { heap_free(data->dir_data.children); }
        if (data->dir_data.dirents) { heap_free(data->dir_data.children); }
        ctx->bytes_used -=
            sizeof(ramfs_data_t) +
            data->dir_data.num_children * sizeof(data->dir_data.children[0]) +
            data->dir_data.num_children * sizeof(data->dir_data.dirents[0]);
        break;
    case RAMFS_DATA_FILE:
        if (data->file_data.buf) { heap_free(data->file_data.buf); }
        ctx->bytes_used -= sizeof(ramfs_data_t) + data->file_data.buf_size;
        break;
    }

    heap_free(data);
}

static bool prv_ramfs_find_child(ramfs_data_t *dir_data, const char *name,
                                 ramfs_data_t **child_data) {
    for (size_t idx = 0; idx < dir_data->dir_data.num_children; idx++) {
        const vfs_dirent_t *const dirent = &dir_data->dir_data.dirents[idx];
        if (string_equals(name, dirent->name)) {
            if (child_data) { *child_data = dir_data->dir_data.children[idx]; }
            return true;
        }
    }
    return false;
}

static vfs_err_t prv_ramfs_add_child(ramfs_ctx_t *ctx, ramfs_data_t *dir_data,
                                     const char *name, ramfs_data_type_t type) {
    ASSERT(dir_data->type == RAMFS_DATA_DIR);

    const size_t new_idx = dir_data->dir_data.num_children;
    const size_t new_len = new_idx + 1;
    ramfs_data_t *const new_data = prv_ramfs_alloc_data(ctx, type);
    if (!new_data) { return VFS_ERR_FS_NO_SPACE; }

    const size_t req_size = sizeof(ramfs_data_t *) + sizeof(vfs_dirent_t);
    if (ctx->bytes_used + req_size > ctx->size) { return VFS_ERR_FS_NO_SPACE; }
    ctx->bytes_used += req_size;

    if (dir_data->dir_data.dirents) {
        dir_data->dir_data.dirents = heap_realloc(
            dir_data->dir_data.dirents, new_len * sizeof(vfs_dirent_t), 4);
    } else {
        dir_data->dir_data.dirents = heap_alloc(new_len * sizeof(vfs_dirent_t));
    }

    if (dir_data->dir_data.children) {
        dir_data->dir_data.children =
            heap_realloc(dir_data->dir_data.children,
                         new_len * sizeof(dir_data->dir_data.children[0]), 4);
    } else {
        dir_data->dir_data.children =
            heap_alloc(new_len * sizeof(ramfs_data_t *));
    }

    dir_data->dir_data.children[new_idx] = new_data;
    kmemcpy(dir_data->dir_data.dirents[new_idx].name, name,
            string_len(name) + 1);
    dir_data->dir_data.num_children = new_len;

    return VFS_ERR_NONE;
}
