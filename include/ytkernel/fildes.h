#pragma once

#include "dynarr.h"

typedef enum {
    FD_ERR_NONE,
    FD_ERR_NO_TASK,
    FD_ERR_FILE_OP,
    FD_ERR_NOT_FOUND,
    FD_ERR_NOT_USED,
} fd_err_t;

typedef struct {
    dynarr_t fds;
} fd_arr_t;

void fd_init_arr(fd_arr_t *arr);

fd_err_t fd_open(const char *path, int flags, size_t *out_fd_idx);
fd_err_t fd_close(size_t fd_idx);

fd_err_t fd_read(size_t fd_idx, void *buf, size_t buf_size, size_t *out_read);
fd_err_t fd_write(size_t fd_idx, const void *buf, size_t buf_size,
                  size_t *out_written);
