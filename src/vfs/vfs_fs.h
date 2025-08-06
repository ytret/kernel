#pragma once

#include "vfs/vfs_err.h"
#include "vfs/vfs_node.h"

typedef struct {
    const char *name;
    vfs_err_t (*f_mount)(void *ctx, vfs_node_t *node);
    vfs_err_t (*f_unmount)(void *ctx, vfs_node_t *node);
} vfs_fs_desc_t;
