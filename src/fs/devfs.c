#include "assert.h"
#include "fs/devfs.h"
#include "heap.h"
#include "kerr.h"
#include "kstring.h"
#include "log.h"
#include "memfun.h"
#include "vfs/vnode.h"

static devfs_node_t *prv_devfs_new_dir(const char *name, devfs_node_t *parent);
static devfs_node_t *
prv_devfs_new_chardev(const char *name, devfs_node_t *parent, void *driver_ctx);
static kerr_t devfs_free_node(devfs_ctx_t *ctx, devfs_node_t *node);

static void prv_devfs_fill_vnode(devfs_ctx_t *ctx, devfs_node_t *node);

static kerr_t prv_devfs_mount(void *v_ctx, vnode_t *vnode);
static kerr_t prv_devfs_unmount(void *v_ctx, vnode_t *vnode);
static void prv_devfs_on_free_vnode(void *v_ctx, vnode_t *vnode);

static const fs_desc_t g_devfs_desc = {
    .name = "devfs",
    .f_mount = prv_devfs_mount,
    .f_unmount = prv_devfs_unmount,
    .f_on_free_vnode = prv_devfs_on_free_vnode,
};

kerr_t prv_devfs_vnode_mknode(vnode_t *vnode, vnode_t **out_vnode,
                              const char *name, vnode_type_t vnode_type);
kerr_t prv_devfs_vnode_rmdir(vnode_t *vnode, const char *name);
kerr_t prv_devfs_vnode_readdir(vnode_t *vnode, void *dirent_buf,
                               size_t buf_items, size_t *out_items);
kerr_t prv_devfs_vnode_lookup(vnode_t *vnode, vnode_t **out_vnode,
                              const char *name);
kerr_t prv_devfs_vnode_read(vnode_t *vnode, size_t offset, void *buf,
                            size_t num_bytes, size_t *out_read);
kerr_t prv_devfs_vnode_write(vnode_t *vnode, size_t offset, const void *buf,
                             size_t num_bytes, size_t *out_written);

static const vnode_ops_t g_devfs_node_ops = {
    .f_mknode = prv_devfs_vnode_mknode,
    .f_rmdir = prv_devfs_vnode_rmdir,
    .f_readdir = prv_devfs_vnode_readdir,
    .f_lookup = prv_devfs_vnode_lookup,
    .f_read = prv_devfs_vnode_read,
    .f_write = prv_devfs_vnode_write,
};

static void *prv_devfs_alloc(void *ctx, size_t size, size_t align);
static void prv_devfs_free(void *ctx, void *ptr, size_t size);

static const dir_tree_alloc_t g_devfs_alloc = {
    .f_alloc = prv_devfs_alloc,
    .f_free = prv_devfs_free,
};

static devfs_ctx_t g_devfs;

devfs_ctx_t *devfs_global_ctx(void) {
    return &g_devfs;
}

void devfs_init(devfs_ctx_t *ctx) {
    kmemset(ctx, 0, sizeof(devfs_ctx_t));

    ctx->root = prv_devfs_new_dir("<root>", NULL);
    ASSERT(ctx->root != NULL);
}

devfs_ctx_t *devfs_new(void) {
    devfs_ctx_t *const ctx =
        heap_alloc_aligned(sizeof(devfs_ctx_t), _Alignof(devfs_ctx_t));
    devfs_init(ctx);
    return ctx;
}

void devfs_deinit(devfs_ctx_t *ctx) {
    (void)ctx;
    LOG_ERROR("TODO");
}

void devfs_free(devfs_ctx_t *ctx) {
    devfs_deinit(ctx);
    heap_free(ctx);
}

const fs_desc_t *devfs_get_desc(void) {
    return &g_devfs_desc;
}

kerr_t devfs_add_chardev(devfs_ctx_t *ctx, const char *name, void *driver_ctx) {
    DEBUG_ASSERT(ctx != NULL);
    DEBUG_ASSERT(name != NULL);
    DEBUG_ASSERT(driver_ctx != NULL);

    devfs_node_t *const root = ctx->root;
    ASSERT(root != NULL);

    if (dir_tree_find_child(&root->dir_node, name, NULL, NULL)) {
        return KERR_EXISTS;
    }

    devfs_node_t *const new_node =
        prv_devfs_new_chardev(name, root, driver_ctx);

    const kerr_t err = dir_tree_add_child(ctx, &g_devfs_alloc, &root->dir_node,
                                          name, &new_node->dir_node);
    if (err != KERR_NONE) {
        const kerr_t free_err = devfs_free_node(ctx, new_node);
        if (free_err != KERR_NONE) {
            PANIC("failed to free chardev node %p: %d", new_node, free_err);
        }
        return err;
    }

    return KERR_NONE;
}

static devfs_node_t *prv_devfs_new_dir(const char *name, devfs_node_t *parent) {
    devfs_node_t *const node =
        heap_alloc_aligned(sizeof(devfs_node_t), _Alignof(devfs_node_t));
    if (!node) { return NULL; }

    kmemset(node, 0, sizeof(*node));
    dir_tree_init(&node->dir_node, DIR_NODE_DIR, name, &parent->dir_node);
    node->type = DEVFS_DIR;

    return node;
}

static devfs_node_t *prv_devfs_new_chardev(const char *name,
                                           devfs_node_t *parent,
                                           void *driver_ctx) {
    devfs_node_t *const node =
        heap_alloc_aligned(sizeof(devfs_node_t), _Alignof(devfs_node_t));
    if (!node) { return NULL; }

    kmemset(node, 0, sizeof(*node));
    dir_tree_init(&node->dir_node, DIR_NODE_LEAF, name, &parent->dir_node);
    node->type = DEVFS_CHAR;
    node->driver_ctx = driver_ctx;

    return node;
}

kerr_t devfs_free_node(devfs_ctx_t *ctx, devfs_node_t *node) {
    if (node->vnode && node->vnode->refcount != 0) { return KERR_IN_USE; }

    bool type_ok = false;
    switch (node->type) {
    case DEVFS_CHAR:
        // FIXME: should we inform the driver?
        type_ok = true;
        break;
    case DEVFS_DIR:
        type_ok = true;
        if (node->dir_node.children.num_items > 0) { return KERR_NOT_EMPTY; }
        // FIXME: do allocation sizes always reflect num_children?
        break;
    }
    ASSERT(type_ok);

    prv_devfs_free(ctx, node, sizeof(*node));

    return KERR_NONE;
}

static void prv_devfs_fill_vnode(devfs_ctx_t *ctx, devfs_node_t *node) {
    if (node->vnode) { PANIC("vnode already set"); }

    vnode_type_t vnode_type;
    bool type_ok = false;
    switch (node->type) {
    case DEVFS_DIR:
        type_ok = true;
        vnode_type = VNODE_DIR;
        break;
    case DEVFS_CHAR:
        type_ok = true;
        vnode_type = VNODE_DEV_CHAR;
        break;
    }
    ASSERT(type_ok);

    vnode_t *const vnode = vnode_get();
    vnode->type = vnode_type;
    vnode->flags = 0;
    vnode->ops = &g_devfs_node_ops;
    vnode->fs_desc = &g_devfs_desc;
    vnode->fs_ctx = ctx;
    vnode->fs_data = node;

    node->vnode = vnode;
}

static kerr_t prv_devfs_mount(void *v_ctx, vnode_t *vnode) {
    LOG_FLOW("v_ctx %p vnode %p", v_ctx, vnode);
    DEBUG_ASSERT(v_ctx != NULL);
    DEBUG_ASSERT(vnode != NULL);

    if (vnode->type != VNODE_DIR) { return KERR_NOT_SUPP; }

    devfs_ctx_t *const ctx = v_ctx;

    // FIXME: should I check if ctx is already mounted somewhere? What about
    // vnode duplication in case of allowing multitple mount points?

    vnode->ops = &g_devfs_node_ops;
    vnode->fs_desc = &g_devfs_desc;
    vnode->fs_ctx = v_ctx;
    vnode->fs_data = ctx->root;

    return KERR_NONE;
}

static kerr_t prv_devfs_unmount(void *v_ctx, vnode_t *vnode) {
    LOG_FLOW("v_ctx %p vnode %p", v_ctx, vnode);
    DEBUG_ASSERT(v_ctx != NULL);
    DEBUG_ASSERT(vnode != NULL);

    if (vnode->type != VNODE_DIR) { return KERR_NOT_SUPP; }

    devfs_ctx_t *const ctx = v_ctx;
    if (vnode->fs_data != ctx->root) { return KERR_NOT_MOUNTED; }

    vnode->fs_ctx = NULL;
    vnode->fs_data = NULL;

    return KERR_NONE;
}

static void prv_devfs_on_free_vnode(void *v_ctx, vnode_t *vnode) {
    LOG_FLOW("v_ctx %p vnode %p", v_ctx, vnode);
    DEBUG_ASSERT(v_ctx != NULL);
    DEBUG_ASSERT(vnode != NULL);

    devfs_node_t *const node = vnode->fs_data;
    // NOTE: `node` can be NULL if the file/dir was deleted while the node still
    // was being referenced somewhere.
    if (node) { node->vnode = NULL; }
}

kerr_t prv_devfs_vnode_mknode(vnode_t *vnode, vnode_t **out_vnode,
                              const char *name, vnode_type_t vnode_type) {
    LOG_FLOW("vnode %p out_vnode %p name %s vnode_type %d", vnode, out_vnode,
             name, vnode_type);

    if (!vnode) { return KERR_BAD_ARGS; }
    if (vnode->type != VNODE_DIR) { return KERR_NOT_SUPP; }
    if (!out_vnode) { return KERR_BAD_ARGS; }
    if (!name) { return KERR_BAD_ARGS; }
    if (string_len(name) + 1 > VFS_MAX_NAME_SIZE) {
        // FIXME: check for overflow in the condition
        return KERR_NAME_TOO_LONG;
    }

    devfs_ctx_t *const ctx = vnode->fs_ctx;
    devfs_node_t *const node = vnode->fs_data;
    if (!ctx) { return KERR_NO_FS; }
    if (!node) { return KERR_NO_FS_DATA; }

    if (dir_tree_find_child(&node->dir_node, name, NULL, NULL)) {
        return KERR_EXISTS;
    }

    devfs_node_t *new_node = NULL;
    bool type_ok = false;
    switch (vnode_type) {
    case VNODE_NONE:
    case VNODE_FILE:
    case VNODE_DEV_CHAR: type_ok = false; break;
    case VNODE_DIR:
        type_ok = true;
        new_node = prv_devfs_new_dir(name, node);
        break;
    }

    if (!type_ok) {
        LOG_ERROR("unsupported new node type %d", vnode_type);
        return KERR_BAD_ARGS;
    }
    if (!new_node) {
        LOG_ERROR("failed to create a child node");
        return KERR_NO_SPACE;
    }

    const kerr_t err = dir_tree_add_child(ctx, &g_devfs_alloc, &node->dir_node,
                                          name, &new_node->dir_node);
    if (err != KERR_NONE) {
        const kerr_t free_err = devfs_free_node(ctx, new_node);
        if (free_err != KERR_NONE) {
            PANIC("failed to free supposedly empty dir node %p: %d", new_node,
                  free_err);
        }
        return err;
    }

    prv_devfs_fill_vnode(ctx, new_node);
    *out_vnode = new_node->vnode;

    return KERR_NONE;
}

kerr_t prv_devfs_vnode_rmdir(vnode_t *vnode, const char *name) {
    LOG_FLOW("vnode %p name %s", vnode, name);

    if (!vnode) { return KERR_BAD_ARGS; }
    if (vnode->type != VNODE_DIR) { return KERR_NOT_SUPP; }
    if (!name) { return KERR_BAD_ARGS; }

    devfs_ctx_t *const ctx = vnode->fs_ctx;
    devfs_node_t *const node = vnode->fs_data;
    if (!ctx) { return KERR_NO_FS; }
    if (!node) { return KERR_NO_FS_DATA; }

    devfs_node_t *child;
    size_t child_idx;
    if (!dir_tree_find_child(&node->dir_node, name, (dir_node_t **)&child,
                             &child_idx)) {
        static_assert(offsetof(devfs_node_t, dir_node) == 0);
        return KERR_NOT_FOUND;
    }

    if (child->type != DEVFS_DIR) { return KERR_NOT_SUPP; }
    if (child->dir_node.children.num_items != 0) { return KERR_NOT_EMPTY; }

    if (child->vnode) {
        child->vnode->fs_data = NULL;
        child->vnode = NULL;
    }

    kerr_t err =
        dir_tree_rm_child(ctx, &g_devfs_alloc, &node->dir_node, child_idx);
    if (err != KERR_NONE) { return err; }

    err = devfs_free_node(ctx, child);
    return err;
}

kerr_t prv_devfs_vnode_readdir(vnode_t *vnode, void *dirent_buf,
                               size_t buf_items, size_t *out_items) {
    LOG_FLOW("vnode %p dirent_buf %p buf_len %zu out_len %p", vnode, dirent_buf,
             buf_items, out_items);

    if (!vnode) { return KERR_BAD_ARGS; }
    if (vnode->type != VNODE_DIR) { return KERR_NOT_SUPP; }
    if (!dirent_buf) { return KERR_BAD_ARGS; }
    if (buf_items == 0) { return KERR_BAD_ARGS; }
    if (!out_items) { return KERR_BAD_ARGS; }

    devfs_ctx_t *const ctx = vnode->fs_ctx;
    devfs_node_t *const node = vnode->fs_data;
    if (!ctx) { return KERR_NO_FS; }
    if (!node) { return KERR_NO_FS_DATA; }

    dirent_t *const dest_dirents = dirent_buf;
    const size_t copy_items = node->dir_node.children.num_items <= buf_items
                                  ? node->dir_node.children.num_items
                                  : buf_items;
    for (size_t idx = 0; idx < copy_items; idx++) {
        dirent_t *idx_dirent;
        const bool get_ok = dynarr_get_at(&node->dir_node.dirents, idx,
                                          &idx_dirent, sizeof(dirent_t *));
        ASSERT(get_ok);
        kmemcpy(&dest_dirents[idx], idx_dirent, sizeof(dirent_t));
    }
    *out_items = copy_items;

    return KERR_NONE;
}

kerr_t prv_devfs_vnode_lookup(vnode_t *vnode, vnode_t **out_vnode,
                              const char *name) {
    LOG_FLOW("vnode %p out_vnode %p name %s", vnode, out_vnode, name);

    if (!vnode) { return KERR_BAD_ARGS; }
    if (vnode->type != VNODE_DIR) { return KERR_NOT_SUPP; }
    if (!out_vnode) { return KERR_BAD_ARGS; }
    if (!name) { return KERR_BAD_ARGS; }
    if (string_len(name) + 1 > VFS_MAX_NAME_SIZE) {
        // FIXME: check for overflow in the condition
        return KERR_NAME_TOO_LONG;
    }

    devfs_ctx_t *const ctx = vnode->fs_ctx;
    devfs_node_t *const node = vnode->fs_data;
    if (!ctx) { return KERR_NO_FS; }
    if (!node) { return KERR_NO_FS_DATA; }

    devfs_node_t *found_node;
    if (!dir_tree_find_child(&node->dir_node, name, (dir_node_t **)&found_node,
                             NULL)) {
        return KERR_NOT_FOUND;
    }

    if (!found_node->vnode) { prv_devfs_fill_vnode(ctx, found_node); }
    *out_vnode = found_node->vnode;

    return KERR_NONE;
}

kerr_t prv_devfs_vnode_read(vnode_t *vnode, size_t offset, void *buf,
                            size_t num_bytes, size_t *out_read) {
    LOG_FLOW("vnode %p offset %zu buf %p num_bytes %zu out_read %p", vnode,
             offset, buf, num_bytes, out_read);

    if (!vnode) { return KERR_BAD_ARGS; }
    if (vnode->type != VNODE_DEV_CHAR) { return KERR_NOT_SUPP; }
    if (!buf) { return KERR_BAD_ARGS; }

    devfs_ctx_t *const ctx = vnode->fs_ctx;
    devfs_node_t *const node = vnode->fs_data;
    if (!ctx) { return KERR_NO_FS; }
    if (!node) { return KERR_NO_FS_DATA; }

    switch (node->type) {
    case DEVFS_DIR:
    case DEVFS_CHAR: {
        chardev_t *chardev = node->driver_ctx;
        ASSERT(chardev != NULL);
        ASSERT(chardev->ops != NULL);
        if (chardev->ops->f_write) {
            return chardev->ops->f_read(chardev->ctx, buf, num_bytes, out_read);
        } else {
            return KERR_NOT_SUPP;
        }
        break;
    }
    }

    return KERR_NOT_SUPP;
}

kerr_t prv_devfs_vnode_write(vnode_t *vnode, size_t offset, const void *buf,
                             size_t num_bytes, size_t *out_written) {
    LOG_FLOW("vnode %p offset %zu buf %p num_bytes %zu out_written %p", vnode,
             offset, buf, num_bytes, out_written);

    if (!vnode) { return KERR_BAD_ARGS; }
    if (vnode->type != VNODE_DEV_CHAR) { return KERR_NOT_SUPP; }
    if (!buf) { return KERR_BAD_ARGS; }

    devfs_ctx_t *const ctx = vnode->fs_ctx;
    devfs_node_t *const node = vnode->fs_data;
    if (!ctx) { return KERR_NO_FS; }
    if (!node) { return KERR_NO_FS_DATA; }

    switch (node->type) {
    case DEVFS_DIR:  return KERR_NOT_SUPP;
    case DEVFS_CHAR: {
        chardev_t *chardev = node->driver_ctx;
        ASSERT(chardev != NULL);
        ASSERT(chardev->ops != NULL);
        if (chardev->ops->f_write) {
            return chardev->ops->f_write(chardev->ctx, buf, num_bytes,
                                         out_written);
        } else {
            return KERR_NOT_SUPP;
        }
        break;
    }
    }

    return KERR_NOT_SUPP;
}

static void *prv_devfs_alloc(void *ctx, size_t size, size_t align) {
    (void)ctx;
    return heap_alloc_aligned(size, align);
}

static void prv_devfs_free(void *ctx, void *ptr, size_t size) {
    (void)ctx;
    (void)size;
    heap_free(ptr);
}
