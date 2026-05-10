#pragma once

#include <stddef.h>

#include "config.h"
#include "dynarr.h"
#include "vfs/fs_desc.h"

typedef struct ramfs_node ramfs_node_t;

typedef enum {
    RAMFS_FILE,
    RAMFS_DIR,
} ramfs_node_type_t;

typedef struct {
    dynarr_t children; // item type: `ramfs_node_t *`
    dynarr_t dirents;  // item type: `dirent_t *`
} ramfs_dir_data_t;

struct ramfs_node {
    char *name;
    ramfs_node_t *parent;
    vnode_t *vnode;

    ramfs_node_type_t type;
    union {
        struct {
            void *buf;
            size_t buf_size;
        } file;
        ramfs_dir_data_t dir;
    };

#ifdef YTKERNEL_ENABLE_TESTS
    _Atomic bool deleted;
#endif
};

typedef struct {
    ramfs_node_t *root;

    size_t used_size;
    size_t allowed_size;
} ramfs_ctx_t;

void ramfs_init(ramfs_ctx_t *ctx, size_t allowed_size);
ramfs_ctx_t *ramfs_new(size_t allowed_size);
const fs_desc_t *ramfs_get_desc(void);
