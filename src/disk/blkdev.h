#pragma once

#include <stddef.h>
#include <stdint.h>

#include "semaphore.h"

typedef struct blkdev_if blkdev_if_t;

typedef enum {
    BLKDEV_OP_READ,
} blkdev_op_t;

typedef struct {
    blkdev_op_t op;
    uint64_t lba;

    const void *read_buf;
    size_t read_size;
    void *write_buf;
    size_t write_size;

    void *dev_ctx;            //!< Device driver context for the interface.
    struct blkdev_if *dev_if; //!< Device driver interface for blkdev.

    void (*cb_done)(semaphore_t *sem_done);
} blkdev_req_t;

struct blkdev_if {
    bool (*f_is_busy)(void *ctx);
    void (*f_submit)(const blkdev_req_t *req);
};

bool blkdev_enqueue_req(blkdev_req_t *req);

[[gnu::noreturn]]
void blkdev_task_entry(void);
