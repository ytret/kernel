#pragma once

#include "vfs_node.h"
#include "vfs_path.h"

typedef enum {
    FILE_RDONLY = 1 << 0,
    FILE_WRONLY = 1 << 1,
    FILE_RDWR = 1 << 2,
    FILE_SEARCH = 1 << 3,
    FILE_EXEC = 1 << 4,
} file_flags_t;

typedef enum {
    FILE_ERR_NONE,
    FILE_ERR_CANNOT_OPEN,
    FILE_ERR_BAD_FLAGS,
    FILE_ERR_NOT_FOUND,
    FILE_ERR_NOT_OPENED,
} file_err_t;

typedef struct {
    bool opened;
    vfs_node_t *node;
    file_flags_t flags;
} file_t;

file_err_t file_open_node(vfs_node_t *node, file_t *file);
file_err_t file_open_path(const vfs_path_t *path, file_t *file);
file_err_t file_open_path_str(const char *path_str, file_t *file);
file_err_t file_close(file_t *file);
