#pragma once

#include "types.h"
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
    FILE_ERR_NOT_SUPP,
    FILE_ERR_NEG_OFFSET,
    FILE_ERR_IO,
} file_err_t;

typedef enum {
    FILE_SEEK_SET,
    FILE_SEEK_CUR,
    FILE_SEEK_END,
} file_seek_t;

typedef struct {
    bool opened;
    vfs_node_t *node;
    file_flags_t flags;
    off_t offset;
} file_t;

file_err_t file_open_node(vfs_node_t *node, file_t *file);
file_err_t file_open_path(const vfs_path_t *path, file_t *file);
file_err_t file_open_path_str(const char *path_str, file_t *file);
file_err_t file_close(file_t *file);

file_err_t file_seek(file_t *file, off_t offset, file_seek_t whence);
file_err_t file_read(file_t *file, void *buf, size_t num_bytes,
                     size_t *out_read);
