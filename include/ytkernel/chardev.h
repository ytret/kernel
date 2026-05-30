#pragma once

#include <stddef.h>

#include "kerr.h"

typedef struct chardev chardev_t;

typedef enum {
    CHARDEV_UNINIT,
    CHARDEV_SERIAL,
    CHARDEV_CONSOLE,
    CHARDEV_TTY,
} chardev_type_t;

typedef struct {
    kerr_t (*f_write)(void *ctx, const void *buf, size_t buf_size,
                      size_t *out_written);
    kerr_t (*f_read)(void *ctx, void *buf, size_t buf_size, size_t *out_read);
} chardev_ops_t;

struct chardev {
    chardev_type_t type;
    void *ctx;
    const chardev_ops_t *ops;
};
