#pragma once

#include <stddef.h>

#include "chardev.h"
#include "kerr.h"

typedef kerr_t (*ldisc_op_write_t)(void *ctx, const void *buf, size_t buf_size,
                                   size_t *num_written);
typedef kerr_t (*ldisc_op_read_t)(void *ctx, void *buf, size_t buf_size,
                                  size_t *num_read);

typedef enum {
    LDISC_UNINIT,
    LDISC_RAW,
    LDISC_COOKED,
} ldisc_type_t;

typedef struct {
    const ldisc_op_write_t f_write;
    const ldisc_op_read_t f_read;
} ldisc_ops_t;

typedef struct {
    ldisc_type_t type;
    void *ctx;
    const ldisc_ops_t *ops;
} ldisc_t;

void ldisc_raw_init(ldisc_t *ldisc);

void ldisc_cooked_init(ldisc_t *ldisc);
void ldisc_cooked_set_echo_dev(ldisc_t *ldisc, chardev_t *dev);

void ldisc_destroy(ldisc_t *ldisc);
