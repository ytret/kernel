#include <limits.h>

#include "heap.h"
#include "log.h"
#include "memfun.h"
#include "panic.h"
#include "vfs/vnode.h"

typedef struct {
    vnode_t *root_node;

#ifdef YTKERNEL_ENABLE_TESTS
    _Atomic size_t vnode_destroy_cnt;
#endif
} vfs_ctx_t;

static vfs_ctx_t g_vfs;

void vnode_root_init(void) {
    g_vfs.root_node = heap_alloc(sizeof(*g_vfs.root_node));
    kmemset(g_vfs.root_node, 0, sizeof(*g_vfs.root_node));
    mutex_init(&g_vfs.root_node->lock);

    g_vfs.root_node->refcount = 1;
    g_vfs.root_node->type = VNODE_DIR;
    g_vfs.root_node->ops = NULL;
}

vnode_t *vnode_root_node(void) {
    return g_vfs.root_node;
}

vnode_t *vnode_get(void) {
    vnode_t *const node = heap_alloc(sizeof(*node));
    kmemset(node, 0, sizeof(*node));
    mutex_init(&node->lock);
    node->refcount = 1;

#ifdef YTKERNEL_ENABLE_TESTS
    node->magic = VNODE_MAGIC_LIVE;
#endif

    return node;
}

void vnode_ref(vnode_t *node) {
    if (node->refcount == INT_MAX) {
        PANIC("vnode_ref called with refcount INT_MAX (%d)", INT_MAX);
    }
    node->refcount++;
}

bool vnode_put(vnode_t *node) {
    if (node->refcount == 0) { PANIC("vnode_put called with refcount 0"); }

    node->refcount--;
    if (node->refcount == 0) {
        LOG_FLOW("free node %p", node);
        if (node->fs_desc && node->fs_desc->f_on_free_vnode) {
            node->fs_desc->f_on_free_vnode(node->fs_ctx, node);
        }
        if (node->lock.waiting_tasks.p_first_node) {
            PANIC("cannot free node which has waiting tasks");
        }
#ifdef YTKERNEL_ENABLE_TESTS
        node->magic = VNODE_MAGIC_DEAD;
        g_vfs.vnode_destroy_cnt++;
#else
        heap_free(node);
#endif
        return true;
    }

    return false;
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

        mutex_acquire(&vfs_node->lock);
        vnode_t *child_node;
        auto f_lookup = vfs_node->ops->f_lookup;
        vfs_err_t err = f_lookup(vfs_node, &child_node, child_name);
        mutex_release(&vfs_node->lock);

        if (err != VFS_ERR_NONE) { return VPATH_ERR_BAD_NODE; }
        vfs_node = child_node;
    }

    *out_node = vfs_node;
    return VPATH_ERR_NONE;
}

#ifdef YTKERNEL_ENABLE_TESTS
size_t vnode_get_destroy_cnt(void) {
    return g_vfs.vnode_destroy_cnt;
}
#endif
