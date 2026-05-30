#define LOG_LEVEL LOG_LEVEL_DEBUG

#include "assert.h"
#include "dynarr.h"
#include "fs/ramfs.h"
#include "heap.h"
#include "kstring.h"
#include "log.h"
#include "memfun.h"
#include "panic.h"
#include "vfs/vnode.h"

#define RAMFS_FILE_BUF_ALIGN 32

static vfs_err_t prv_ramfs_fs_mount(void *v_ctx, vnode_t *vnode);
static vfs_err_t prv_ramfs_fs_unmount(void *v_ctx, vnode_t *vnode);
static void prv_ramfs_on_free_vnode(void *v_ctx, vnode_t *vnode);

static const fs_desc_t g_ramfs_desc = {
    .name = "ramfs",
    .f_mount = prv_ramfs_fs_mount,
    .f_unmount = prv_ramfs_fs_unmount,
    .f_on_free_vnode = prv_ramfs_on_free_vnode,
};

vfs_err_t prv_ramfs_vnode_mknode(vnode_t *vnode, vnode_t **out_vnode,
                                 const char *name, vnode_type_t vnode_type);
vfs_err_t prv_ramfs_vnode_unlink(vnode_t *vnode, const char *name);
vfs_err_t prv_ramfs_vnode_rmdir(vnode_t *vnode, const char *name);
vfs_err_t prv_ramfs_vnode_readdir(vnode_t *vnode, void *dirent_buf,
                                  size_t buf_items, size_t *out_items);
vfs_err_t prv_ramfs_vnode_lookup(vnode_t *vnode, vnode_t **out_vnode,
                                 const char *name);
vfs_err_t prv_ramfs_vnode_read(vnode_t *vnode, size_t offset, void *buf,
                               size_t num_bytes, size_t *out_read);
vfs_err_t prv_ramfs_vnode_write(vnode_t *vnode, size_t offset, const void *buf,
                                size_t num_bytes, size_t *out_written);

static const vnode_ops_t g_ramfs_node_ops = {
    .f_mknode = prv_ramfs_vnode_mknode,
    .f_unlink = prv_ramfs_vnode_unlink,
    .f_rmdir = prv_ramfs_vnode_rmdir,
    .f_readdir = prv_ramfs_vnode_readdir,
    .f_lookup = prv_ramfs_vnode_lookup,
    .f_read = prv_ramfs_vnode_read,
    .f_write = prv_ramfs_vnode_write,
};

static ramfs_node_t *prv_ramfs_new_dir(ramfs_ctx_t *ctx, const char *name,
                                       ramfs_node_t *parent);
static ramfs_node_t *prv_ramfs_new_file(ramfs_ctx_t *ctx, const char *name,
                                        ramfs_node_t *parent);

static void prv_ramfs_fill_vnode(ramfs_ctx_t *ctx, ramfs_node_t *node);
static vnode_type_t prv_ramfs_vnode_type(ramfs_node_type_t node_type);

static void *prv_ramfs_alloc(void *v_ctx, size_t size, size_t align);
static void prv_ramfs_free(void *v_ctx, void *ptr, size_t size);

static const dir_tree_alloc_t g_ramfs_alloc = {
    .f_alloc = prv_ramfs_alloc,
    .f_free = prv_ramfs_free,
};

void ramfs_init(ramfs_ctx_t *ctx, size_t allowed_size) {
    DEBUG_ASSERT(ctx != NULL);
    ASSERT(allowed_size != 0);

    kmemset(ctx, 0, sizeof(*ctx));
    ctx->allowed_size = allowed_size;

    // NOTE: this call relies on some of the fields to be initialized.
    ctx->root = prv_ramfs_new_dir(ctx, "<root>", NULL);
    if (!ctx->root) { PANIC("failed to create ramfs root node"); }
}

ramfs_ctx_t *ramfs_new(size_t allowed_size) {
    ramfs_ctx_t *const ctx = heap_alloc(sizeof(ramfs_ctx_t));
    ramfs_init(ctx, allowed_size);
    return ctx;
}

void ramfs_deinit(ramfs_ctx_t *ctx) {
    // TODO
    (void)ctx;
}

void ramfs_free(ramfs_ctx_t *ctx) {
    ramfs_deinit(ctx);
    heap_free(ctx);
}

const fs_desc_t *ramfs_get_desc(void) {
    return &g_ramfs_desc;
}

vfs_err_t ramfs_free_node(ramfs_ctx_t *ctx, ramfs_node_t *node) {
    if (node->vnode && node->vnode->refcount != 0) { return VFS_ERR_NODE_USED; }

#ifdef YTKERNEL_ENABLE_TESTS
    (void)ctx;
    node->deleted = true;
#else
    bool type_ok = false;
    switch (node->type) {
    case RAMFS_FILE:
        type_ok = true;
        prv_ramfs_free(ctx, node->file.buf, node->file.buf_size);
        break;
    case RAMFS_DIR:
        type_ok = true;
        if (node->dir.children.num_items > 0) { return VFS_ERR_NODE_NOT_EMPTY; }
        // FIXME: do allocation sizes always reflect num_children?
        break;
    }
    ASSERT(type_ok);

    prv_ramfs_free(ctx, node, sizeof(*node));
#endif

    return VFS_ERR_NONE;
}

static vfs_err_t prv_ramfs_fs_mount(void *v_ctx, vnode_t *vnode) {
    LOG_FLOW("v_ctx %p vnode %p", v_ctx, vnode);
    DEBUG_ASSERT(v_ctx != NULL);
    DEBUG_ASSERT(vnode != NULL);

    if (vnode->type != VNODE_DIR) { return VFS_ERR_NODE_NOT_DIR; }

    ramfs_ctx_t *const ctx = v_ctx;

    // FIXME: should I check if ctx is already mounted somewhere? What about
    // vnode duplication in case of allowing multitple mount points?

    vnode->ops = &g_ramfs_node_ops;
    vnode->fs_desc = &g_ramfs_desc;
    vnode->fs_ctx = v_ctx;
    vnode->fs_data = ctx->root;

    return VFS_ERR_NONE;
}

static vfs_err_t prv_ramfs_fs_unmount(void *v_ctx, vnode_t *vnode) {
    LOG_FLOW("v_ctx %p vnode %p", v_ctx, vnode);
    DEBUG_ASSERT(v_ctx != NULL);
    DEBUG_ASSERT(vnode != NULL);

    if (vnode->type != VNODE_DIR) { return VFS_ERR_NODE_NOT_DIR; }

    ramfs_ctx_t *const ctx = v_ctx;
    if (vnode->fs_data != ctx->root) { return VFS_ERR_NODE_NOT_MOUNTED; }

    vnode->fs_ctx = NULL;
    vnode->fs_data = NULL;

    return VFS_ERR_NONE;
}

static void prv_ramfs_on_free_vnode(void *v_ctx, vnode_t *vnode) {
    LOG_FLOW("v_ctx %p vnode %p", v_ctx, vnode);
    DEBUG_ASSERT(v_ctx != NULL);
    DEBUG_ASSERT(vnode != NULL);

    ramfs_node_t *const node = vnode->fs_data;
    // NOTE: `node` can be NULL if the file/dir was deleted while the node still
    // was being referenced somewhere.
    if (node) { node->vnode = NULL; }
}

vfs_err_t prv_ramfs_vnode_mknode(vnode_t *vnode, vnode_t **out_vnode,
                                 const char *name, vnode_type_t vnode_type) {
    LOG_FLOW("vnode %p out_vnode %p name %s vnode_type %d", vnode, out_vnode,
             name, vnode_type);

    if (!vnode) { return VFS_ERR_NODE_BAD_ARGS; }
    if (vnode->type != VNODE_DIR) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!out_vnode) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!name) { return VFS_ERR_NODE_BAD_ARGS; }
    if (string_len(name) + 1 > VFS_MAX_NAME_SIZE) {
        // FIXME: check for overflow in the condition
        return VFS_ERR_NODE_NAME_TOO_LONG;
    }

    ramfs_ctx_t *const ctx = vnode->fs_ctx;
    ramfs_node_t *const node = vnode->fs_data;
    if (!ctx) { return VFS_ERR_NODE_NO_FS; }
    if (!node) { return VFS_ERR_NODE_NO_DATA; }

    if (dir_tree_find_child(&node->dir_node, name, NULL, NULL)) {
        return VFS_ERR_NAME_TAKEN;
    }

    ramfs_node_t *new_node = NULL;
    bool type_ok = false;
    switch (vnode_type) {
    case VNODE_NONE:
    case VNODE_DEV_CHAR: type_ok = false; break;
    case VNODE_DIR:
        type_ok = true;
        new_node = prv_ramfs_new_dir(ctx, name, node);
        break;
    case VNODE_FILE:
        type_ok = true;
        new_node = prv_ramfs_new_file(ctx, name, node);
        break;
    }

    if (!type_ok) {
        LOG_ERROR("invalid parent node type %d", vnode_type);
        return VFS_ERR_NODE_BAD_ARGS;
    }
    if (!new_node) {
        LOG_ERROR("failed to create a child node");
        return VFS_ERR_FS_NO_SPACE;
    }

    const vfs_err_t err = dir_tree_add_child(
        ctx, &g_ramfs_alloc, &node->dir_node, name, &new_node->dir_node);
    if (err != VFS_ERR_NONE) {
        const vfs_err_t free_err = ramfs_free_node(ctx, new_node);
        if (free_err != VFS_ERR_NONE) {
            PANIC("failed to free supposedly empty dir node %p: %d", new_node,
                  free_err);
        }
        return err;
    }

    prv_ramfs_fill_vnode(ctx, new_node);
    *out_vnode = new_node->vnode;

    return VFS_ERR_NONE;
}

vfs_err_t prv_ramfs_vnode_unlink(vnode_t *vnode, const char *name) {
    LOG_FLOW("vnode %p name %s", vnode, name);

    if (!vnode) { return VFS_ERR_NODE_BAD_ARGS; }
    if (vnode->type != VNODE_DIR) { return VFS_ERR_NODE_NOT_DIR; }
    if (!name) { return VFS_ERR_NODE_BAD_ARGS; }

    ramfs_ctx_t *const ctx = vnode->fs_ctx;
    ramfs_node_t *const node = vnode->fs_data;
    if (!ctx) { return VFS_ERR_NODE_NO_FS; }
    if (!node) { return VFS_ERR_NODE_NO_DATA; }

    ramfs_node_t *child;
    size_t child_idx;
    if (!dir_tree_find_child(&node->dir_node, name, (dir_node_t **)&child,
                             &child_idx)) {
        static_assert(offsetof(ramfs_node_t, dir_node) == 0);
        return VFS_ERR_NODE_NOT_FOUND;
    }

    if (child->type != RAMFS_FILE) { return VFS_ERR_NODE_NOT_FILE; }

    if (child->vnode) {
        child->vnode->fs_data = NULL;
        child->vnode = NULL;
    }

    return dir_tree_rm_child(ctx, &g_ramfs_alloc, &node->dir_node, child_idx);
}

vfs_err_t prv_ramfs_vnode_rmdir(vnode_t *vnode, const char *name) {
    LOG_FLOW("vnode %p name %s", vnode, name);

    if (!vnode) { return VFS_ERR_NODE_BAD_ARGS; }
    if (vnode->type != VNODE_DIR) { return VFS_ERR_NODE_NOT_DIR; }
    if (!name) { return VFS_ERR_NODE_BAD_ARGS; }

    ramfs_ctx_t *const ctx = vnode->fs_ctx;
    ramfs_node_t *const node = vnode->fs_data;
    if (!ctx) { return VFS_ERR_NODE_NO_FS; }
    if (!node) { return VFS_ERR_NODE_NO_DATA; }

    ramfs_node_t *child;
    size_t child_idx;
    if (!dir_tree_find_child(&node->dir_node, name, (dir_node_t **)&child,
                             &child_idx)) {
        static_assert(offsetof(ramfs_node_t, dir_node) == 0);
        return VFS_ERR_NODE_NOT_FOUND;
    }

    if (child->type != RAMFS_DIR) { return VFS_ERR_NODE_NOT_DIR; }
    if (child->dir_node.children.num_items != 0) {
        return VFS_ERR_NODE_NOT_EMPTY;
    }

    if (child->vnode) {
        child->vnode->fs_data = NULL;
        child->vnode = NULL;
    }

    return dir_tree_rm_child(ctx, &g_ramfs_alloc, &node->dir_node, child_idx);
}

vfs_err_t prv_ramfs_vnode_readdir(vnode_t *vnode, void *dirent_buf,
                                  size_t buf_items, size_t *out_items) {
    LOG_FLOW("vnode %p dirent_buf %p buf_len %zu out_len %p", vnode, dirent_buf,
             buf_items, out_items);

    if (!vnode) { return VFS_ERR_NODE_BAD_ARGS; }
    if (vnode->type != VNODE_DIR) { return VFS_ERR_NODE_NOT_DIR; }
    if (!dirent_buf) { return VFS_ERR_NODE_BAD_ARGS; }
    if (buf_items == 0) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!out_items) { return VFS_ERR_NODE_BAD_ARGS; }

    ramfs_ctx_t *const ctx = vnode->fs_ctx;
    ramfs_node_t *const node = vnode->fs_data;
    if (!ctx) { return VFS_ERR_NODE_NO_FS; }
    if (!node) { return VFS_ERR_NODE_NO_DATA; }

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

    return VFS_ERR_NONE;
}

vfs_err_t prv_ramfs_vnode_lookup(vnode_t *vnode, vnode_t **out_vnode,
                                 const char *name) {
    LOG_FLOW("vnode %p out_vnode %p name %s", vnode, out_vnode, name);

    if (!vnode) { return VFS_ERR_NODE_BAD_ARGS; }
    if (vnode->type != VNODE_DIR) { return VFS_ERR_NODE_NOT_DIR; }
    if (!out_vnode) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!name) { return VFS_ERR_NODE_BAD_ARGS; }
    if (string_len(name) + 1 > VFS_MAX_NAME_SIZE) {
        // FIXME: check for overflow in the condition
        return VFS_ERR_NODE_NAME_TOO_LONG;
    }

    ramfs_ctx_t *const ctx = vnode->fs_ctx;
    ramfs_node_t *const node = vnode->fs_data;
    if (!ctx) { return VFS_ERR_NODE_NO_FS; }
    if (!node) { return VFS_ERR_NODE_NO_DATA; }

    ramfs_node_t *found_node;
    if (!dir_tree_find_child(&node->dir_node, name, (dir_node_t **)&found_node,
                             NULL)) {
        return VFS_ERR_NODE_NOT_FOUND;
    }

    if (!found_node->vnode) { prv_ramfs_fill_vnode(ctx, found_node); }
    *out_vnode = found_node->vnode;

    return VFS_ERR_NONE;
}

vfs_err_t prv_ramfs_vnode_read(vnode_t *vnode, size_t offset, void *buf,
                               size_t num_bytes, size_t *out_read) {
    LOG_FLOW("vnode %p offset %zu buf %p num_bytes %zu out_read %p", vnode,
             offset, buf, num_bytes, out_read);

    if (!vnode) { return VFS_ERR_NODE_BAD_ARGS; }
    if (vnode->type != VNODE_FILE) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!buf) { return VFS_ERR_NODE_BAD_ARGS; }

    ramfs_ctx_t *const ctx = vnode->fs_ctx;
    ramfs_node_t *const node = vnode->fs_data;
    if (!ctx) { return VFS_ERR_NODE_NO_FS; }
    if (!node) { return VFS_ERR_NODE_NO_DATA; }

    ASSERT(node->type == RAMFS_FILE);
    const size_t file_size = node->file.buf_size;
    if (offset >= file_size) { return VFS_ERR_NODE_BAD_OFFSET; }

    // FIXME: check for overflow
    if (offset + num_bytes >= file_size) { num_bytes = file_size - offset; }

    kmemcpy(buf, (void *)((uintptr_t)node->file.buf + offset), num_bytes);
    if (out_read) { *out_read = num_bytes; }

    return VFS_ERR_NONE;
}

vfs_err_t prv_ramfs_vnode_write(vnode_t *vnode, size_t offset, const void *buf,
                                size_t num_bytes, size_t *out_written) {
    LOG_FLOW("vnode %p offset %zu buf %p num_bytes %zu out_written %p", vnode,
             offset, buf, num_bytes, out_written);

    if (!vnode) { return VFS_ERR_NODE_BAD_ARGS; }
    if (vnode->type != VNODE_FILE) { return VFS_ERR_NODE_BAD_ARGS; }
    if (!buf) { return VFS_ERR_NODE_BAD_ARGS; }

    ramfs_ctx_t *const ctx = vnode->fs_ctx;
    ramfs_node_t *const node = vnode->fs_data;
    if (!ctx) { return VFS_ERR_NODE_NO_FS; }
    if (!node) { return VFS_ERR_NODE_NO_DATA; }

    ASSERT(node->type == RAMFS_FILE);

    const size_t old_size = node->file.buf_size;
    const size_t write_end = offset + num_bytes;
    const size_t new_size = write_end > old_size ? write_end : old_size;
    if (new_size > old_size) {
        node->file.buf =
            heap_realloc(node->file.buf, new_size, RAMFS_FILE_BUF_ALIGN);
        node->file.buf_size = new_size;

        kmemset((void *)((uintptr_t)node->file.buf + old_size), 0,
                new_size - old_size);
    }

    kmemcpy((void *)((uintptr_t)node->file.buf + offset), buf, num_bytes);
    if (out_written) { *out_written = num_bytes; }

    return VFS_ERR_NONE;
}

static ramfs_node_t *prv_ramfs_new_dir(ramfs_ctx_t *ctx, const char *name,
                                       ramfs_node_t *parent) {
    ramfs_node_t *const node =
        prv_ramfs_alloc(ctx, sizeof(ramfs_node_t), _Alignof(ramfs_node_t));
    if (!node) { return NULL; }

    kmemset(node, 0, sizeof(*node));
    dir_tree_init(&node->dir_node, DIR_NODE_DIR, name, &parent->dir_node);
    node->type = RAMFS_DIR;

    return node;
}

static ramfs_node_t *prv_ramfs_new_file(ramfs_ctx_t *ctx, const char *name,
                                        ramfs_node_t *parent) {
    ramfs_node_t *const node =
        prv_ramfs_alloc(ctx, sizeof(ramfs_node_t), _Alignof(ramfs_node_t));
    if (!node) { return NULL; }

    kmemset(node, 0, sizeof(*node));
    dir_tree_init(&node->dir_node, DIR_NODE_LEAF, name, &parent->dir_node);
    node->type = RAMFS_FILE;

    return node;
}

static void prv_ramfs_fill_vnode(ramfs_ctx_t *ctx, ramfs_node_t *node) {
    if (node->vnode) { PANIC("vnode already set"); }

    const vnode_type_t vnode_type = prv_ramfs_vnode_type(node->type);

    vnode_t *const vnode = vnode_get();
    vnode->type = vnode_type;
    vnode->flags = 0;
    vnode->ops = &g_ramfs_node_ops;
    vnode->fs_desc = &g_ramfs_desc;
    vnode->fs_ctx = ctx;
    vnode->fs_data = node;

    node->vnode = vnode;
}

static vnode_type_t prv_ramfs_vnode_type(ramfs_node_type_t node_type) {
    switch (node_type) {
    case RAMFS_DIR:  return VNODE_DIR;
    case RAMFS_FILE: return VNODE_FILE;
    }
    PANIC("not implemented for ramfs type %d", node_type);
}

static void *prv_ramfs_alloc(void *v_ctx, size_t size, size_t align) {
    ramfs_ctx_t *const ctx = v_ctx;

    // TODO: check for overflow
    const size_t new_size = ctx->used_size + size;
    if (new_size > ctx->allowed_size) {
        LOG_ERROR(
            "failed to allocate %zu bytes for ramfs %p: %zu used / %zu allowed",
            size, ctx, ctx->used_size, ctx->allowed_size);
        return NULL;
    }

    void *const ptr = heap_alloc_aligned(size, align);
    ctx->used_size = new_size;
    return ptr;
}

static void prv_ramfs_free(void *v_ctx, void *ptr, size_t size) {
    ramfs_ctx_t *const ctx = v_ctx;

    ASSERT(size <= ctx->used_size);
    heap_free(ptr);
    ctx->used_size -= size;
}
