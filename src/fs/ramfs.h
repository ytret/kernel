#pragma once

#include <stddef.h>

#include "vfs/fs_desc.h"

typedef struct ramfs_node ramfs_node_t;

typedef struct {
    ramfs_node_t *root;

    size_t used_size;
    size_t allowed_size;
} ramfs_ctx_t;

void ramfs_init(ramfs_ctx_t *ctx, size_t allowed_size);
ramfs_ctx_t *ramfs_new(size_t allowed_size);
const fs_desc_t *ramfs_get_desc(void);
