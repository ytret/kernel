#include "heap.h"
#include "memfun.h"
#include "vfs/vnode.h"

typedef struct {
    vnode_t *root_node;
} vfs_ctx_t;

static vfs_ctx_t g_vfs;

void vnode_root_init(void) {
    g_vfs.root_node = heap_alloc(sizeof(*g_vfs.root_node));
    kmemset(g_vfs.root_node, 0, sizeof(*g_vfs.root_node));

    g_vfs.root_node->type = VNODE_DIR;
    g_vfs.root_node->ops = NULL;
}

vnode_t *vnode_root_node(void) {
    return g_vfs.root_node;
}

vnode_t *vnode_alloc(void) {
    vnode_t *const node = heap_alloc(sizeof(*node));
    kmemset(node, 0, sizeof(*node));
    return node;
}

void vnode_free(vnode_t *node) {
    heap_free(node);
}

vpath_err_t vnode_resolve_path_str(const char *path_str, vnode_t **out_node) {
    vpath_t path;
    vpath_err_t err = vpath_from_str(path_str, &path);
    if (err != VPATH_ERR_NONE) { return err; }

    return vnode_resolve_path(&path, out_node);
}

vpath_err_t vnode_resolve_path(const vpath_t *path, vnode_t **out_node) {
    if (!path->is_absolute) { return VPATH_ERR_MUST_BE_ABSOLUTE; }

    vnode_t *vfs_node = g_vfs.root_node;
    for (list_node_t *list_node = path->parts.p_first_node; list_node != NULL;
         list_node = list_node->p_next) {
        const vpath_part_t *const path_part =
            LIST_NODE_TO_STRUCT(list_node, vpath_part_t, list_node);

        const char *const child_name = path_part->name;

        if (!vfs_node->ops) { return VPATH_ERR_BAD_NODE; }
        if (!vfs_node->ops->f_lookup) { return VPATH_ERR_BAD_NODE; }

        vnode_t *child_node;
        auto f_lookup = vfs_node->ops->f_lookup;
        vfs_err_t err = f_lookup(vfs_node, &child_node, child_name);
        if (err != VFS_ERR_NONE) { return VPATH_ERR_BAD_NODE; }

        vfs_node = child_node;
    }

    *out_node = vfs_node;
    return VPATH_ERR_NONE;
}
