#include "assert.h"
#include "fildes.h"
#include "vfs/file.h"

typedef struct {
    bool used;
    file_t file;
} fd_t;

static fd_t *prv_fd_get_next(fd_arr_t *arr, size_t *fd_idx);
static fd_t *prv_fd_get_by_idx(fd_arr_t *arr, size_t fd_idx);

void fd_init_arr(fd_arr_t *arr) {
    dynarr_init(&arr->fds, sizeof(fd_t), _Alignof(fd_t), 0);
}

fd_err_t fd_open(const char *path, int flags, size_t *out_fd_idx) {
    task_t *const task = taskmgr_local_running_task();
    if (!task) { return -FD_ERR_NO_TASK; }

    size_t fd_idx;
    fd_t *const fd = prv_fd_get_next(&task->fd_arr, &fd_idx);
    fd->file.flags = flags;

    const file_err_t err = file_open_path_str(path, &fd->file);
    if (err != FILE_ERR_NONE) {
        // FIXME: common errno?
        return FD_ERR_FILE_OP;
    }

    fd->used = true;
    if (out_fd_idx) { *out_fd_idx = fd_idx; }
    return FD_ERR_NONE;
}

fd_err_t fd_close(size_t fd_idx) {
    task_t *const task = taskmgr_local_running_task();
    if (!task) { return FD_ERR_NO_TASK; }

    fd_t *const fd = prv_fd_get_by_idx(&task->fd_arr, fd_idx);
    if (!fd) { return FD_ERR_NOT_FOUND; }
    if (!fd->used) { return FD_ERR_NOT_USED; }

    const file_err_t err = file_close(&fd->file);
    if (err != FILE_ERR_NONE) {
        // FIXME: common errno?
        return FD_ERR_FILE_OP;
    }

    fd->used = false;
    return FD_ERR_NONE;
}

fd_err_t fd_read(size_t fd_idx, void *buf, size_t buf_size, size_t *out_read) {
    task_t *const task = taskmgr_local_running_task();
    if (!task) { return FD_ERR_NO_TASK; }

    fd_t *const fd = prv_fd_get_by_idx(&task->fd_arr, fd_idx);
    if (!fd) { return FD_ERR_NOT_FOUND; }
    if (!fd->used) { return FD_ERR_NOT_USED; }

    const file_err_t err = file_read(&fd->file, buf, buf_size, out_read);
    if (err != FILE_ERR_NONE) {
        // FIXME: common errno?
        return FD_ERR_FILE_OP;
    }

    return FD_ERR_NONE;
}

fd_err_t fd_write(size_t fd_idx, const void *buf, size_t buf_size,
                  size_t *out_written) {
    task_t *const task = taskmgr_local_running_task();
    if (!task) { return FD_ERR_NO_TASK; }

    fd_t *const fd = prv_fd_get_by_idx(&task->fd_arr, fd_idx);
    if (!fd) { return FD_ERR_NOT_FOUND; }
    if (!fd->used) { return FD_ERR_NOT_USED; }

    const file_err_t err = file_write(&fd->file, buf, buf_size, out_written);
    if (err != FILE_ERR_NONE) {
        // FIXME: common errno?
        return FD_ERR_FILE_OP;
    }

    return FD_ERR_NONE;
}

static fd_t *prv_fd_get_next(fd_arr_t *arr, size_t *fd_idx) {
    size_t idx;
    const fd_t fd = {0};

    const bool push_ok = dynarr_push(&arr->fds, &fd, &idx);
    ASSERT(push_ok);

    fd_t *const new_fd = dynarr_ptr_at(&arr->fds, idx);
    ASSERT(new_fd != NULL);

    if (fd_idx) { *fd_idx = idx; }

    return new_fd;
}

static fd_t *prv_fd_get_by_idx(fd_arr_t *arr, size_t fd_idx) {
    return dynarr_ptr_at(&arr->fds, fd_idx);
}
