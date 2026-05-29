/**
 * Device filesystem API.
 */

#pragma once

#include <stddef.h>

#include "vfs/dir_tree.h"
#include "vfs/fs_desc.h"

typedef struct devfs_node devfs_node_t;

typedef enum {
    DEVFS_DIR,
    DEVFS_CHAR,
} devfs_node_type_t;

struct devfs_node {
    dir_node_t dir_node;
    vnode_t *vnode;

    devfs_node_type_t type;
    void *driver_ctx;
};

typedef struct {
    devfs_node_t *root;
} devfs_ctx_t;

devfs_ctx_t *devfs_global_ctx(void);

void devfs_init(devfs_ctx_t *ctx);
devfs_ctx_t *devfs_new(void);
void devfs_deinit(devfs_ctx_t *ctx);
void devfs_free(devfs_ctx_t *ctx);

const fs_desc_t *devfs_get_desc(void);

vfs_err_t devfs_add_chardev(devfs_ctx_t *ctx, const char *name,
                            void *driver_ctx);
