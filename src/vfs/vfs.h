#pragma once

#include "vfs/vfs_node.h"

typedef struct {
    vfs_node_t *root_node;
} vfs_ctx_t;

void vfs_init(void);
vfs_node_t *vfs_root_node(void);
vfs_node_t *vfs_alloc_node(void);
