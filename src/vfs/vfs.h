#pragma once

#include "vfs/vnode.h"
#include "vfs/vpath.h"

void vfs_init(void);
vnode_t *vfs_root_node(void);

vnode_t *vfs_alloc_node(void);
void vfs_free_node(vnode_t *node);

vpath_err_t vfs_resolve_path_str(const char *path_str, vnode_t **out_node);
vpath_err_t vfs_resolve_path(const vpath_t *path, vnode_t **out_node);
