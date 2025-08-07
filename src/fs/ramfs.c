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
    ramfs_data_type_t type;
    union {
        struct {
            void *buf;
            size_t buf_size;
        } file_data;
        struct {
            vfs_dirent_t *dirents;
            size_t dirents_size;
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
static bool prv_ramfs_is_name_taken(vfs_node_t *dir_node, const char *name);
static void prv_ramfs_add_dirent(ramfs_data_t *dir_data, const char *name);

ramfs_ctx_t *ramfs_init(size_t num_bytes) {
    ramfs_ctx_t *const ctx = heap_alloc(sizeof(*ctx));
    kmemset(ctx, 0, sizeof(*ctx));

    ctx->bytes_used = 0;
    ctx->size = num_bytes;

    ctx->root = prv_ramfs_alloc_data(ctx, RAMFS_DATA_DIR);

    return ctx;
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

    if (node->fs_ctx) { return VFS_ERR_NODE_ALREADY_MOUNTED; }
    if (node->type != VFS_NODE_DIR) { return VFS_ERR_NODE_NOT_DIR; }

    node->flags |= VFS_NODE_ROOT;
    node->ops = &g_ramfs_node_ops;
    node->fs_ctx = ctx;
    node->fs_data = ctx->root;

    return VFS_ERR_NONE;
}

vfs_err_t ramfs_unmount(void *v_ctx, vfs_node_t *node) {
    ramfs_ctx_t *const ctx = v_ctx;

    if (node->fs_ctx != ctx) { return VFS_ERR_NODE_NOT_MOUNTED; }

    node->flags &= ~VFS_NODE_ROOT;
    node->ops = NULL;
    node->fs_ctx = NULL;
    node->fs_data = NULL;

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

    if (prv_ramfs_is_name_taken(dir_node, name)) { return VFS_ERR_NAME_TAKEN; }

    ramfs_data_t *new_data;
    switch (node_type) {
    case VFS_NODE_DIR:
        new_data = prv_ramfs_alloc_data(ctx, RAMFS_DATA_DIR);
        break;
    case VFS_NODE_FILE:
        new_data = prv_ramfs_alloc_data(ctx, RAMFS_DATA_FILE);
        break;
    default:
        return VFS_ERR_NODE_BAD_ARGS;
    }
    if (!new_data) { return VFS_ERR_NO_SPACE; }

    vfs_node_t *const new_node = vfs_alloc_node();
    new_node->type = node_type;
    new_node->flags = 0;
    new_node->ops = &g_ramfs_node_ops;
    new_node->fs_ctx = ctx;
    new_node->fs_data = new_data;

    prv_ramfs_add_dirent(data, name);

    *out_node = new_node;

    return VFS_ERR_NONE;
}

vfs_err_t ramfs_node_readdir(vfs_node_t *node, void *dirent_buf,
                             size_t buf_size, size_t *out_size) {
    if (!node) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!dirent_buf) { return VFS_ERR_NODE_BAD_ARGS; }
    if (buf_size == 0) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!out_size) { return VFS_ERR_NODE_BAD_ARGS; }

    if (node->type != VFS_NODE_DIR) { return VFS_ERR_NODE_NOT_DIR; }

    ramfs_ctx_t *const ctx = node->fs_ctx;
    ramfs_data_t *const data = node->fs_data;
    if (!ctx) { return VFS_ERR_NODE_NO_FS; }
    if (!data) { return VFS_ERR_NODE_NO_DATA; }

    const size_t copy_size = data->dir_data.dirents_size <= buf_size
                                 ? data->dir_data.dirents_size
                                 : buf_size;
    kmemcpy(dirent_buf, data->dir_data.dirents, copy_size);
    *out_size = copy_size;

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
    return data;
}

static bool prv_ramfs_is_name_taken(vfs_node_t *dir_node, const char *name) {
    ramfs_data_t *const data = dir_node->fs_data;
    for (size_t idx = 0;
         idx < data->dir_data.dirents_size / sizeof(vfs_dirent_t); idx++) {
        const vfs_dirent_t *const dirent = &data->dir_data.dirents[idx];
        if (string_equals(name, dirent->name)) { return true; }
    }
    return false;
}

static void prv_ramfs_add_dirent(ramfs_data_t *dir_data, const char *name) {
    ASSERT(dir_data->type == RAMFS_DATA_DIR);

    const size_t new_idx =
        dir_data->dir_data.dirents_size / sizeof(vfs_dirent_t);
    const size_t new_size =
        dir_data->dir_data.dirents_size + sizeof(vfs_dirent_t);

    if (dir_data->dir_data.dirents) {
        dir_data->dir_data.dirents =
            heap_realloc(dir_data->dir_data.dirents, new_size, 4);
    } else {
        dir_data->dir_data.dirents = heap_alloc(new_size);
    }
    kmemcpy(dir_data->dir_data.dirents[new_idx].name, name,
            string_len(name) + 1);
    dir_data->dir_data.dirents_size = new_size;
}
