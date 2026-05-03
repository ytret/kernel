#include "assert.h"
#include "log.h"
#include "vfs/file.h"
#include "vfs/vfs.h"

#define FILE_FLAGS_FILE (FILE_RDONLY | FILE_WRONLY | FILE_RDWR | FILE_EXEC)
#define FILE_FLAGS_DIR  (FILE_SEARCH)

file_err_t file_open_node(vfs_node_t *node, file_t *file) {
    DEBUG_ASSERT(node);
    DEBUG_ASSERT(file);

    bool ok_type = false;
    int bad_flags;
    switch (node->type) {
    case VFS_NODE_NONE:
        LOG_ERROR("invalid node type VFS_NODE_NONE");
        return FILE_ERR_CANNOT_OPEN;
    case VFS_NODE_FILE:
        bad_flags = file->flags & ~FILE_FLAGS_FILE;
        if (bad_flags) {
            LOG_ERROR("bad flags for file: 0x%08x", bad_flags);
            return FILE_ERR_BAD_FLAGS;
        }
        ok_type = true;
        break;
    case VFS_NODE_DIR:
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

    return FILE_ERR_NONE;
}

file_err_t file_open_path(const vfs_path_t *path, file_t *file) {
    vfs_node_t *node;
    vfs_err_t err = vfs_resolve_path(path, &node);
    if (err != VFS_ERR_NONE) {
        LOG_ERROR("failed to resolve path: %s", vfs_err_str(err));
        return FILE_ERR_NOT_FOUND;
    }
    return file_open_node(node, file);
}

file_err_t file_open_path_str(const char *path_str, file_t *file) {
    vfs_node_t *node;
    vfs_err_t err = vfs_resolve_path_str(path_str, &node);
    if (err != VFS_ERR_NONE) {
        LOG_ERROR("failed to resolve path '%s': %s", path_str,
                  vfs_err_str(err));
        return FILE_ERR_NOT_FOUND;
    }
    return file_open_node(node, file);
}

file_err_t file_close(file_t *file) {
    DEBUG_ASSERT(file);

    if (!file->opened) { return FILE_ERR_NOT_OPENED; }

    file->opened = false;

    return FILE_ERR_NONE;
}
