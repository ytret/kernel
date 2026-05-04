#include "assert.h"
#include "kinttypes.h"
#include "log.h"
#include "vfs/file.h"
#include "vfs/vfs.h"

#define FILE_FLAGS_FILE (FILE_RDONLY | FILE_WRONLY | FILE_RDWR | FILE_EXEC)
#define FILE_FLAGS_DIR  (FILE_SEARCH)

file_err_t file_open_node(vnode_t *node, file_t *file) {
    LOG_FLOW("node %p file %p", node, file);
    DEBUG_ASSERT(node != NULL);
    DEBUG_ASSERT(file != NULL);

    bool ok_type = false;
    int bad_flags;
    switch (node->type) {
    case VNODE_NONE:
        LOG_ERROR("invalid node type VFS_NODE_NONE");
        return FILE_ERR_CANNOT_OPEN;
    case VNODE_FILE:
        bad_flags = file->flags & ~FILE_FLAGS_FILE;
        if (bad_flags) {
            LOG_ERROR("bad flags for file: 0x%08x", bad_flags);
            return FILE_ERR_BAD_FLAGS;
        }
        ok_type = true;
        break;
    case VNODE_DIR:
        bad_flags = file->flags & ~FILE_FLAGS_DIR;
        if (bad_flags) {
            LOG_ERROR("bad flags for directory: 0x%08x", bad_flags);
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

    return FILE_ERR_NONE;
}

file_err_t file_open_path(const vfs_path_t *path, file_t *file) {
    LOG_FLOW("path %p file %p", path, file);
    DEBUG_ASSERT(path != NULL);
    DEBUG_ASSERT(file != NULL);

    vnode_t *node;
    vfs_err_t err = vfs_resolve_path(path, &node);
    if (err != VFS_ERR_NONE) {
        LOG_ERROR("failed to resolve path: %s", vfs_err_str(err));
        return FILE_ERR_NOT_FOUND;
    }
    return file_open_node(node, file);
}

file_err_t file_open_path_str(const char *path_str, file_t *file) {
    LOG_FLOW("path_str %s file %p", path_str, file);
    DEBUG_ASSERT(path_str != NULL);
    DEBUG_ASSERT(file != NULL);

    vnode_t *node;
    vfs_err_t err = vfs_resolve_path_str(path_str, &node);
    if (err != VFS_ERR_NONE) {
        LOG_ERROR("failed to resolve path '%s': %s", path_str,
                  vfs_err_str(err));
        return FILE_ERR_NOT_FOUND;
    }
    return file_open_node(node, file);
}

file_err_t file_close(file_t *file) {
    LOG_FLOW("file %p", file);
    DEBUG_ASSERT(file != NULL);

    if (!file->opened) {
        LOG_ERROR("file %p not opened", file);
        return FILE_ERR_NOT_OPENED;
    }

    file->opened = false;

    return FILE_ERR_NONE;
}

file_err_t file_seek(file_t *file, off_t offset, file_seek_t whence) {
    LOG_FLOW("file %p offset %" PRIdOFF " whence %d", file, offset, whence);
    DEBUG_ASSERT(file != NULL);

    if (!file->opened) {
        LOG_ERROR("file %p not opened", file);
        return FILE_ERR_NOT_OPENED;
    }
    if (file->node->type != VNODE_FILE) {
        LOG_ERROR("node type %d does not support seek", file->node->type);
        return FILE_ERR_NOT_SUPP;
    }

    off_t new_offset;
    switch (whence) {
    case FILE_SEEK_SET:
        new_offset = offset;
        break;
    case FILE_SEEK_CUR:
        new_offset = file->offset + offset;
        break;
    case FILE_SEEK_END:
        PANIC("TODO FILE_SEEK_END");
        break;
    }

    if (new_offset < 0) {
        LOG_ERROR("negative offset (%" PRIdOFF ")", new_offset);
        return FILE_ERR_NEG_OFFSET;
    }

    return FILE_ERR_NONE;
}

file_err_t file_read(file_t *file, void *buf, size_t num_bytes,
                     size_t *out_read) {
    LOG_FLOW("file %p buf %p num_bytes %zu out_read %p", file, buf, num_bytes,
             out_read);
    DEBUG_ASSERT(file != NULL);
    DEBUG_ASSERT(buf != NULL);
    DEBUG_ASSERT(out_read != NULL);

    if (!file->opened) {
        LOG_ERROR("file %p not opened", file);
        return FILE_ERR_NOT_OPENED;
    }
    if (file->node->type != VNODE_FILE) {
        LOG_ERROR("node %p type %d does not support read", file->node,
                  file->node->type);
        return FILE_ERR_NOT_SUPP;
    }
    if (!file->node->ops->f_read) {
        LOG_ERROR("node %p file system does not support read", file->node);
        return FILE_ERR_NOT_SUPP;
    }

    ASSERT(file->offset >= 0);

    vfs_err_t err = file->node->ops->f_read(file->node, (size_t)file->offset,
                                            buf, num_bytes, out_read);
    if (err == VFS_ERR_NONE) {
        return FILE_ERR_NONE;
    } else {
        LOG_ERROR("node %p read returned %d: %s", file->node, err,
                  vfs_err_str(err));
        return FILE_ERR_IO;
    }
}
