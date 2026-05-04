#pragma once

#include <stddef.h>

#include "vfs/vfs_err.h"
#include "vfs/vpath.h"

#define VNODE_MAX_NAME_SIZE 256

typedef struct vnode vnode_t;

typedef struct {
    char name[VNODE_MAX_NAME_SIZE];
} dirent_t;

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
} vnode_ops_t;

struct vnode {
    vnode_type_t type;
    vnode_flags_t flags;
    const vnode_ops_t *ops;
    size_t size;

    void *fs_ctx;  //!< File system context, shared by all of its nodes.
    void *fs_data; //!< File-system-specific data, referenced only by this node.
};

void vnode_root_init(void);
vnode_t *vnode_root_node(void);

vnode_t *vnode_alloc(void);
void vnode_free(vnode_t *node);

vpath_err_t vnode_resolve_path_str(const char *path_str, vnode_t **out_node);
vpath_err_t vnode_resolve_path(const vpath_t *path, vnode_t **out_node);
