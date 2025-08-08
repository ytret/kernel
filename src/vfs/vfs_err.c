#include "vfs/vfs_err.h"

const char *vfs_err_str(vfs_err_t err) {
    switch (err) {
    case VFS_ERR_NONE:
        return "VFS_ERR_NONE";
    case VFS_ERR_FS_NO_SPACE:
        return "VFS_ERR_FS_NO_SPACE";
    case VFS_ERR_FS_ALREADY_MOUNTED:
        return "VFS_ERR_FS_ALREADY_MOUNTED";
    case VFS_ERR_FS_CORRUPTED:
        return "VFS_ERR_FS_CORRUPTED";
    case VFS_ERR_NODE_BAD_OP:
        return "VFS_ERR_NODE_BAD_OP";
    case VFS_ERR_NODE_BAD_ARGS:
        return "VFS_ERR_NODE_BAD_ARGS";
    case VFS_ERR_NODE_NOT_DIR:
        return "VFS_ERR_NODE_NOT_DIR";
    case VFS_ERR_NODE_ALREADY_MOUNTED:
        return "VFS_ERR_NODE_ALREADY_MOUNTED";
    case VFS_ERR_NODE_NOT_MOUNTED:
        return "VFS_ERR_NODE_NOT_MOUNTED";
    case VFS_ERR_NODE_NO_FS:
        return "VFS_ERR_NODE_NO_FS";
    case VFS_ERR_NODE_NO_DATA:
        return "VFS_ERR_NODE_NO_DATA";
    case VFS_ERR_NODE_NAME_TOO_LONG:
        return "VFS_ERR_NODE_NAME_TOO_LONG";
    case VFS_ERR_NODE_NOT_FOUND:
        return "VFS_ERR_NODE_NOT_FOUND";
    case VFS_ERR_PATH_EMPTY:
        return "VFS_ERR_PATH_EMPTY";
    case VFS_ERR_PATH_TOO_MANY_PARTS:
        return "VFS_ERR_PATH_TOO_MANY_PARTS";
    case VFS_ERR_PATH_PART_TOO_LONG:
        return "VFS_ERR_PATH_PART_TOO_LONG";
    case VFS_ERR_PATH_MUST_BE_ABSOLUTE:
        return "VFS_ERR_PATH_MUST_BE_ABSOLUTE";
    case VFS_ERR_NAME_TAKEN:
        return "VFS_ERR_NAME_TAKEN";
    }
    return "<unknown error>";
}
