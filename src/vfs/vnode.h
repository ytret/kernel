#pragma once

#include <stddef.h>

#include "vfs/vfs_err.h"

#define VNODE_MAX_NAME_SIZE 256

typedef struct vnode vnode_t;

typedef struct {
    char name[VNODE_MAX_NAME_SIZE];
} vfs_dirent_t;

typedef enum {
    VNODE_NONE,
    VNODE_DIR,
    VNODE_FILE,
} vnode_type_t;

typedef enum {
    VNODE_ROOT = 1 << 0,
} vnode_flags_t;

typedef struct {
    vfs_err_t (*f_mknode)(vnode_t *dir_node, vnode_t **out_node,
                          const char *name, vnode_type_t node_type);
    vfs_err_t (*f_readdir)(vnode_t *node, void *dirent_buf, size_t buf_len,
                           size_t *out_len);
    vfs_err_t (*f_lookup)(vnode_t *dir_node, vnode_t **out_node,
                          const char *name);
    vfs_err_t (*f_read)(vnode_t *node, size_t offset, void *buf,
                        size_t num_bytes, size_t *out_read);
} vfs_node_ops_t;

struct vnode {
    vnode_type_t type;
    vnode_flags_t flags;
    const vfs_node_ops_t *ops;
    size_t size;

    void *fs_ctx;
    void *fs_data;
};
