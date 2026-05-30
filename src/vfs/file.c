#include "assert.h"
#include "kinttypes.h"
#include "log.h"
#include "vfs/file.h"
#include "vfs/vnode.h"

#define FILE_FLAGS_FILE     (FILE_RDONLY | FILE_WRONLY | FILE_RDWR | FILE_EXEC)
#define FILE_FLAGS_DIR      (FILE_SEARCH)
#define FILE_FLAGS_DEV_CHAR FILE_FLAGS_FILE

file_err_t file_open_node(vnode_t *node, file_t *file) {
    LOG_FLOW("node %p file %p", node, file);
    DEBUG_ASSERT(node != NULL);
    DEBUG_ASSERT(file != NULL);

    mutex_init(&file->lock);

    mutex_acquire(&node->lock);

    bool ok_type = false;
    int bad_flags;
    switch (node->type) {
    case VNODE_NONE:
        LOG_ERROR("invalid node type VNODE_NONE");
        mutex_release(&node->lock);
        return FILE_ERR_CANNOT_OPEN;
    case VNODE_FILE:
        bad_flags = file->flags & ~FILE_FLAGS_FILE;
        if (bad_flags) {
            LOG_ERROR("bad flags for file: 0x%08x", bad_flags);
            mutex_release(&node->lock);
            return FILE_ERR_BAD_FLAGS;
        }
        ok_type = true;
        break;
    case VNODE_DIR:
        bad_flags = file->flags & ~FILE_FLAGS_DIR;
        if (bad_flags) {
            LOG_ERROR("bad flags for directory: 0x%08x", bad_flags);
            mutex_release(&node->lock);
            return FILE_ERR_BAD_FLAGS;
        }
        ok_type = true;
        break;
    case VNODE_DEV_CHAR:
        bad_flags = file->flags & ~FILE_FLAGS_DEV_CHAR;
        if (bad_flags) {
            LOG_ERROR("bad flags for character device: 0x%08x", bad_flags);
            mutex_release(&node->lock);
            return FILE_ERR_BAD_FLAGS;
        }
        ok_type = true;
        break;
    }
    if (!ok_type) { PANIC("unchecked node type %d", node->type); }

    file->opened = true;
    file->node = node;
    // file->flags are set already.
    file->offset = 0;

    mutex_release(&node->lock);

    return FILE_ERR_NONE;
}

file_err_t file_open_path(const vpath_t *path, file_t *file) {
    LOG_FLOW("path %p file %p", path, file);
    DEBUG_ASSERT(path != NULL);
    DEBUG_ASSERT(file != NULL);

    vnode_t *node;
    vpath_err_t err = vnode_resolve_path(path, &node);
    if (err != VPATH_ERR_NONE) {
        LOG_ERROR("failed to resolve path: %s", vpath_err_str(err));
        return FILE_ERR_NOT_FOUND;
    }
    return file_open_node(node, file);
}

file_err_t file_open_path_str(const char *path_str, file_t *file) {
    LOG_FLOW("path_str %s file %p", path_str, file);
    DEBUG_ASSERT(path_str != NULL);
    DEBUG_ASSERT(file != NULL);

    vnode_t *node;
    vpath_err_t err = vnode_resolve_path_str(path_str, &node);
    if (err != VPATH_ERR_NONE) {
        LOG_ERROR("failed to resolve path '%s': %s", path_str,
                  vpath_err_str(err));
        return FILE_ERR_NOT_FOUND;
    }
    return file_open_node(node, file);
}

file_err_t file_close(file_t *file) {
    LOG_FLOW("file %p", file);
    DEBUG_ASSERT(file != NULL);

    mutex_acquire(&file->lock);

    if (!file->opened) {
        LOG_ERROR("file %p not opened", file);
        mutex_release(&file->lock);
        return FILE_ERR_NOT_OPENED;
    }

    file->opened = false;

    vnode_t *const node = file->node;
    mutex_acquire(&node->lock);
    if (!vnode_put(node)) { mutex_release(&node->lock); }

    mutex_release(&file->lock);
    return FILE_ERR_NONE;
}

file_err_t file_seek(file_t *file, off_t offset, file_seek_t whence) {
    LOG_FLOW("file %p offset %" PRIdOFF " whence %d", file, offset, whence);
    DEBUG_ASSERT(file != NULL);

    mutex_acquire(&file->lock);

    if (!file->opened) {
        LOG_ERROR("file %p not opened", file);
        mutex_release(&file->lock);
        return FILE_ERR_NOT_OPENED;
    }

    vnode_t *const node = file->node;
    mutex_acquire(&node->lock);
    if (node->type != VNODE_FILE) {
        LOG_ERROR("node type %d does not support seek", node->type);
        mutex_release(&node->lock);
        mutex_release(&file->lock);
        return FILE_ERR_NOT_SUPP;
    }
    mutex_release(&node->lock);

    bool whence_ok = false;
    off_t new_offset;
    switch (whence) {
    case FILE_SEEK_SET:
        whence_ok = true;
        new_offset = offset;
        break;
    case FILE_SEEK_CUR:
        whence_ok = true;
        new_offset = file->offset + offset;
        break;
    case FILE_SEEK_END:
        whence_ok = true;
        PANIC("TODO FILE_SEEK_END");
        break;
    }
    if (!whence_ok) {
        mutex_release(&file->lock);
        return FILE_ERR_BAD_ARGS;
    }

    if (new_offset < 0) {
        LOG_ERROR("negative offset (%" PRIdOFF ")", new_offset);
        mutex_release(&file->lock);
        return FILE_ERR_NEG_OFFSET;
    }

    mutex_release(&file->lock);
    return FILE_ERR_NONE;
}

file_err_t file_read(file_t *file, void *buf, size_t num_bytes,
                     size_t *out_read) {
    LOG_FLOW("file %p buf %p num_bytes %zu out_read %p", file, buf, num_bytes,
             out_read);
    DEBUG_ASSERT(file != NULL);
    DEBUG_ASSERT(buf != NULL);

    mutex_acquire(&file->lock);

    if (!file->opened) {
        LOG_ERROR("file %p not opened", file);
        mutex_release(&file->lock);
        return FILE_ERR_NOT_OPENED;
    }

    vnode_t *const node = file->node;
    mutex_acquire(&node->lock);
    if (node->type != VNODE_FILE && node->type != VNODE_DEV_CHAR) {
        LOG_ERROR("node %p type %d does not support read", node, node->type);
        mutex_release(&node->lock);
        mutex_release(&file->lock);
        return FILE_ERR_NOT_SUPP;
    }
    if (!node->ops->f_read) {
        LOG_ERROR("node %p file system does not support read", node);
        mutex_release(&node->lock);
        mutex_release(&file->lock);
        return FILE_ERR_NOT_SUPP;
    }

    ASSERT(file->offset >= 0);

    const vfs_err_t err =
        node->ops->f_read(node, (size_t)file->offset, buf, num_bytes, out_read);

    mutex_release(&node->lock);
    mutex_release(&file->lock);

    if (err == VFS_ERR_NONE) {
        return FILE_ERR_NONE;
    } else {
        LOG_ERROR("node %p read returned %d: %s", node, err, vfs_err_str(err));
        return FILE_ERR_IO;
    }
}

file_err_t file_write(file_t *file, const void *buf, size_t num_bytes,
                      size_t *out_written) {
    LOG_FLOW("file %p buf %p num_bytes %zu out_written %p", file, buf,
             num_bytes, out_written);
    DEBUG_ASSERT(file != NULL);
    DEBUG_ASSERT(buf != NULL);

    mutex_acquire(&file->lock);

    if (!file->opened) {
        LOG_ERROR("file %p not opened", file);
        mutex_release(&file->lock);
        return FILE_ERR_NOT_OPENED;
    }

    vnode_t *const node = file->node;
    mutex_acquire(&node->lock);
    if (node->type != VNODE_FILE && node->type != VNODE_DEV_CHAR) {
        LOG_ERROR("node %p type %d does not support write", node, node->type);
        mutex_release(&node->lock);
        mutex_release(&file->lock);
        return FILE_ERR_NOT_SUPP;
    }
    if (!node->ops->f_write) {
        LOG_ERROR("node %p file system does not support write", node);
        mutex_release(&node->lock);
        mutex_release(&file->lock);
        return FILE_ERR_NOT_SUPP;
    }

    ASSERT(file->offset >= 0);

    const vfs_err_t err = node->ops->f_write(node, (size_t)file->offset, buf,
                                             num_bytes, out_written);

    mutex_release(&node->lock);
    mutex_release(&file->lock);

    if (err == VFS_ERR_NONE) {
        return FILE_ERR_NONE;
    } else {
        LOG_ERROR("node %p read returned %d: %s", node, err, vfs_err_str(err));
        return FILE_ERR_IO;
    }
}
