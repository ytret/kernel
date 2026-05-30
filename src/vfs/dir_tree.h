#pragma once

#include "dynarr.h"
#include "kerr.h"

typedef enum {
    DIR_NODE_DIR,
    DIR_NODE_LEAF,
} dir_node_type_t;

typedef struct dir_node {
    dir_node_type_t type;
    char *name;
    struct dir_node *parent;

    dynarr_t children; // item type: `ramfs_node_t *`
    dynarr_t dirents;  // item type: `dirent_t *`
} dir_node_t;

typedef void *(*dir_tree_alloc_fn)(void *ctx, size_t size, size_t align);
typedef void (*dir_tree_free_fn)(void *ctx, void *ptr, size_t size);

typedef struct {
    const dir_tree_alloc_fn f_alloc;
    const dir_tree_free_fn f_free;
} dir_tree_alloc_t;

void dir_tree_init(dir_node_t *node, dir_node_type_t type, const char *name,
                   dir_node_t *parent);

bool dir_tree_find_child(dir_node_t *dir_node, const char *name,
                         dir_node_t **child_node, size_t *child_idx);
kerr_t dir_tree_add_child(void *alloc_ctx, const dir_tree_alloc_t *alloc,
                          dir_node_t *dir_node, const char *child_name,
                          dir_node_t *child_node);
kerr_t dir_tree_rm_child(void *alloc_ctx, const dir_tree_alloc_t *alloc,
                         dir_node_t *dir_node, size_t child_idx);
