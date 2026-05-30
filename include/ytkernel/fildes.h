#pragma once

#include "dynarr.h"
#include "kerr.h"

typedef struct {
    dynarr_t fds;
} fd_arr_t;

void fd_init_arr(fd_arr_t *arr);

kerr_t fd_open(const char *path, int flags, size_t *out_fd_idx);
kerr_t fd_close(size_t fd_idx);

kerr_t fd_read(size_t fd_idx, void *buf, size_t buf_size, size_t *out_read);
kerr_t fd_write(size_t fd_idx, const void *buf, size_t buf_size,
                size_t *out_written);
