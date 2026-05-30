#pragma once

#include <stddef.h>

typedef struct chardev chardev_t;

typedef enum {
    CHARDEV_UNINIT,
    CHARDEV_SERIAL,
    CHARDEV_CONSOLE,
    CHARDEV_TTY,
} chardev_type_t;

typedef struct {
    int (*f_write)(void *ctx, const void *buf, size_t buf_size);
    int (*f_read)(void *ctx, void *buf, size_t buf_size);
} chardev_ops_t;

struct chardev {
    chardev_type_t type;
    void *ctx;
    const chardev_ops_t *ops;
};
