#pragma once

#include <stddef.h>

#include "chardev.h"

typedef struct {
    size_t (*const f_write)(void *ctx, const void *buf, size_t buf_size);
    size_t (*const f_read)(void *ctx, void *buf, size_t buf_size);
} ldisc_ops_t;

typedef struct {
    void *ctx;
    const ldisc_ops_t *ops;
} ldisc_t;

void ldisc_raw_init(ldisc_t *ldisc);

void ldisc_cooked_init(ldisc_t *ldisc);
void ldisc_cooked_set_echo_dev(ldisc_t *ldisc, chardev_t *dev);
