#pragma once

#include <stddef.h>

#include "vfs/vfs_err.h"

#define VFS_NODE_MAX_NAME_SIZE 256

typedef struct vfs_node vfs_node_t;

typedef struct {
    char name[VFS_NODE_MAX_NAME_SIZE];
} vfs_dirent_t;

typedef enum {
    VFS_NODE_NONE,
    VFS_NODE_DIR,
    VFS_NODE_FILE,
} vfs_node_type_t;

typedef enum {
    VFS_NODE_ROOT = 1 << 0,
} vfs_node_flags_t;

typedef struct {
    vfs_err_t (*f_mknode)(vfs_node_t *dir_node, vfs_node_t **out_node,
                          const char *name, vfs_node_type_t node_type);
    vfs_err_t (*f_readdir)(vfs_node_t *node, void *dirent_buf, size_t buf_len,
                           size_t *out_len);
} vfs_node_ops_t;

struct vfs_node {
    vfs_node_type_t type;
    vfs_node_flags_t flags;
    const vfs_node_ops_t *ops;

    void *fs_ctx;
    void *fs_data;
};
