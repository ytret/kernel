#pragma once

#include "vfs/vfs_node.h"
#include "vfs/vfs_path.h"

void vfs_init(void);
vfs_node_t *vfs_root_node(void);

vfs_node_t *vfs_alloc_node(void);
void vfs_free_node(vfs_node_t *node);

vfs_err_t vfs_resolve_path_str(const char *path_str, vfs_node_t **out_node);
vfs_err_t vfs_resolve_path(const vfs_path_t *path, vfs_node_t **out_node);
