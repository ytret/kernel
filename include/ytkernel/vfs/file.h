#pragma once

#include "kmutex.h"
#include "types.h"
#include "vfs/file_err.h"
#include "vfs/vnode.h"
#include "vfs/vpath.h"

typedef enum {
    FILE_RDONLY = 1 << 0,
    FILE_WRONLY = 1 << 1,
    FILE_RDWR = 1 << 2,
    FILE_SEARCH = 1 << 3,
    FILE_EXEC = 1 << 4,
} file_flags_t;

typedef enum {
    FILE_SEEK_SET,
    FILE_SEEK_CUR,
    FILE_SEEK_END,
} file_seek_t;

typedef struct {
    task_mutex_t lock;
    bool opened;
    vnode_t *node;
    file_flags_t flags;
    off_t offset;
} file_t;

file_err_t file_open_node(vnode_t *node, file_t *file);
file_err_t file_open_path(const vpath_t *path, file_t *file);
file_err_t file_open_path_str(const char *path_str, file_t *file);
file_err_t file_close(file_t *file);

file_err_t file_seek(file_t *file, off_t offset, file_seek_t whence);
file_err_t file_read(file_t *file, void *buf, size_t num_bytes,
                     size_t *out_read);
file_err_t file_write(file_t *file, const void *buf, size_t num_bytes,
                      size_t *out_written);
