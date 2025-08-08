#include "heap.h"
#include "memfun.h"
#include "vfs/vfs.h"

typedef struct {
    vfs_node_t *root_node;
} vfs_ctx_t;

static vfs_ctx_t g_vfs;

void vfs_init(void) {
    g_vfs.root_node = heap_alloc(sizeof(*g_vfs.root_node));
    kmemset(g_vfs.root_node, 0, sizeof(*g_vfs.root_node));

    g_vfs.root_node->type = VFS_NODE_DIR;
    g_vfs.root_node->ops = NULL;
}

vfs_node_t *vfs_root_node(void) {
    return g_vfs.root_node;
}

vfs_node_t *vfs_alloc_node(void) {
    vfs_node_t *const node = heap_alloc(sizeof(*node));
    kmemset(node, 0, sizeof(*node));
    return node;
}

void vfs_free_node(vfs_node_t *node) {
    heap_free(node);
}

vfs_err_t vfs_resolve_path_str(const char *path_str, vfs_node_t **out_node) {
    vfs_err_t err;
    vfs_path_t path;

    err = vfs_path_from_str(path_str, &path);
    if (err != VFS_ERR_NONE) { return err; }

    err = vfs_resolve_path(&path, out_node);
    return err;
}

vfs_err_t vfs_resolve_path(const vfs_path_t *path, vfs_node_t **out_node) {
    if (!path->is_absolute) { return VFS_ERR_PATH_MUST_BE_ABSOLUTE; }

    vfs_node_t *vfs_node = g_vfs.root_node;
    for (list_node_t *list_node = path->parts.p_first_node; list_node != NULL;
         list_node = list_node->p_next) {
        const vfs_path_part_t *const path_part =
            LIST_NODE_TO_STRUCT(list_node, vfs_path_part_t, list_node);

        const char *const child_name = path_part->name;

        if (!vfs_node->ops) { return VFS_ERR_NODE_BAD_OP; }
        if (!vfs_node->ops->f_lookup) { return VFS_ERR_NODE_BAD_OP; }

        vfs_node_t *child_node;
        auto f_lookup = vfs_node->ops->f_lookup;
        vfs_err_t err = f_lookup(vfs_node, &child_node, child_name);
        if (err != VFS_ERR_NONE) { return err; }

        vfs_node = child_node;
    }

    *out_node = vfs_node;
    return VFS_ERR_NONE;
}
