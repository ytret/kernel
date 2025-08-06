#include "fs/ramfs.h"
#include "heap.h"
#include "memfun.h"

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
    .f_readdir = ramfs_node_readdir,
};

static ramfs_data_t *prv_ramfs_alloc_dir(ramfs_ctx_t *ctx);

ramfs_ctx_t *ramfs_init(size_t num_bytes) {
    ramfs_ctx_t *const ctx = heap_alloc(sizeof(*ctx));
    kmemset(ctx, 0, sizeof(*ctx));

    ctx->bytes_used = 0;
    ctx->size = num_bytes;

    ctx->root = prv_ramfs_alloc_dir(ctx);

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

static ramfs_data_t *prv_ramfs_alloc_dir(ramfs_ctx_t *ctx) {
    const size_t req_size = sizeof(ramfs_data_t);
    if (ctx->bytes_used + req_size > ctx->size) {
        // out of space
        return NULL;
    }

    ramfs_data_t *const node = heap_alloc(sizeof(*node));
    kmemset(node, 0, sizeof(*node));

    node->type = RAMFS_DATA_DIR;
    node->dir_data.dirents = NULL;
    node->dir_data.dirents_size = 0;

    return node;
}
