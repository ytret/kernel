#include "heap.h"
#include "memfun.h"
#include "vfs/vfs.h"

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
