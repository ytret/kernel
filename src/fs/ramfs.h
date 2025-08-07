#pragma once

#include <stddef.h>

#include "vfs/vfs_fs.h"

typedef struct ramfs_data ramfs_data_t;

typedef struct {
    ramfs_data_t *root;
    size_t size;
    size_t bytes_used;
} ramfs_ctx_t;

ramfs_ctx_t *ramfs_init(size_t num_bytes);
void ramfs_free(ramfs_ctx_t *ctx);
const vfs_fs_desc_t *ramfs_get_desc(void);

vfs_err_t ramfs_mount(void *v_ctx, vfs_node_t *node);
vfs_err_t ramfs_unmount(void *v_ctx, vfs_node_t *node);

vfs_err_t ramfs_node_mknode(vfs_node_t *dir_node, vfs_node_t **out_node,
                            const char *name, vfs_node_type_t node_type);
vfs_err_t ramfs_node_readdir(vfs_node_t *node, void *dirent_buf,
                             size_t buf_size, size_t *out_size);
