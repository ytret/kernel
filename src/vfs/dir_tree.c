#include "assert.h"
#include "dir_tree.h"
#include "kstring.h"
#include "memfun.h"
#include "vfs/vnode.h"

void dir_tree_init(dir_node_t *node, dir_node_type_t type, const char *name,
                   dir_node_t *parent) {
    kmemset(node, 0, sizeof(*node));
    node->type = type;
    node->name = string_dup(name);
    node->parent = parent;

    dynarr_init(&node->children, sizeof(dir_node_t *), _Alignof(dir_node_t *),
                0);
    dynarr_init(&node->dirents, sizeof(dirent_t *), _Alignof(dirent_t *), 0);
}

bool dir_tree_find_child(dir_node_t *dir, const char *name,
                         dir_node_t **child_node, size_t *child_idx) {
    ASSERT(dir->type == DIR_NODE_DIR);

    for (size_t idx = 0; idx < dir->children.num_items; idx++) {
        dirent_t *dirent;
        bool get_ok =
            dynarr_get_at(&dir->dirents, idx, &dirent, sizeof(dirent_t *));
        ASSERT(get_ok);

        if (string_equals(name, dirent->name)) {
            if (child_node) {
                bool get_ok = dynarr_get_at(&dir->children, idx, child_node,
                                            sizeof(dir_node_t *));
                ASSERT(get_ok);
            }
            if (child_idx) { *child_idx = idx; }
            return true;
        }
    }

    return false;
}

vfs_err_t dir_tree_add_child(void *alloc_ctx, const dir_tree_alloc_t *alloc,
                             dir_node_t *dir, const char *child_name,
                             dir_node_t *child_node) {
    ASSERT(dir->type == DIR_NODE_DIR);

    dir_node_t *const new_child_node = child_node;
    dirent_t *const new_dirent =
        alloc->f_alloc(alloc_ctx, sizeof(dirent_t), _Alignof(dirent_t));
    if (!new_dirent) { return VFS_ERR_FS_NO_SPACE; }

    kmemcpy(new_dirent->name, child_name, string_len(child_name) + 1);

    bool push_ok = dynarr_push(&dir->children, &new_child_node, NULL);
    if (!push_ok) {
        alloc->f_free(alloc_ctx, new_dirent, sizeof(dirent_t));
        return VFS_ERR_FS_NO_SPACE;
    }

    push_ok = dynarr_push(&dir->dirents, &new_dirent, NULL);
    if (!push_ok) {
        dynarr_take_at(&dir->children, dir->children.num_items - 1, NULL, 0);
        alloc->f_free(alloc_ctx, new_dirent, sizeof(dirent_t));
        return VFS_ERR_FS_NO_SPACE;
    }

    DEBUG_ASSERT(dir->children.num_items == dir->dirents.num_items);

    return VFS_ERR_NONE;
}

vfs_err_t dir_tree_rm_child(void *alloc_ctx, const dir_tree_alloc_t *alloc,
                            dir_node_t *dir, size_t child_idx) {
    DEBUG_ASSERT(alloc_ctx != NULL);
    DEBUG_ASSERT(alloc != NULL);
    DEBUG_ASSERT(dir != NULL);
    ASSERT(dir->type == DIR_NODE_DIR);

    dynarr_take_at(&dir->children, child_idx, NULL, 0);

    dirent_t *rm_dirent;
    dynarr_take_at(&dir->dirents, child_idx, &rm_dirent, sizeof(dirent_t *));
    alloc->f_free(alloc_ctx, rm_dirent, sizeof(dirent_t));

    return VFS_ERR_NONE;
}
