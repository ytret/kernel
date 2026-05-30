#pragma once

#include "dynarr.h"
#include "vfs/file_err.h"

typedef struct {
    dynarr_t fds;
} fd_arr_t;

void fd_init_arr(fd_arr_t *arr);

file_err_t fd_open(const char *path, int flags, size_t *out_fd_idx);
file_err_t fd_close(size_t fd_idx);

file_err_t fd_read(size_t fd_idx, void *buf, size_t buf_size, size_t *out_read);
file_err_t fd_write(size_t fd_idx, const void *buf, size_t buf_size,
                    size_t *out_written);
