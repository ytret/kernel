#pragma once

#include <stddef.h>
#include <stdint.h>

#include "semaphore.h"

typedef struct blkdev_if blkdev_if_t;
typedef struct blkdev_req blkdev_req_t;

struct blkdev_if {
    bool (*f_is_busy)(void *ctx);
    void (*f_submit_req)(blkdev_req_t *req);
};

typedef enum {
    BLKDEV_OP_READ,
    BLKDEV_OP_WRITE,
} blkdev_op_t;

typedef enum {
    BLKDEV_REQ_INACTIVE,
    BLKDEV_REQ_ACTIVE,
    BLKDEV_REQ_ERROR,
    BLKDEV_REQ_SUCCESS,
} blkdev_req_state_t;

struct blkdev_req {
    _Atomic blkdev_req_state_t state;
    blkdev_op_t op;
    uint64_t start_sector;

    void *read_buf;
    size_t read_sectors;
    const void *write_buf;
    size_t write_sectors;

    void *dev_ctx;      //!< Device driver context for the interface.
    blkdev_if_t dev_if; //!< Device driver interface for blkdev.

    semaphore_t sem_done;
};

/**
 * Enqueues the request @a req.
 *
 * @warning
 * Lifetime of @a *req must be long enough for a driver to fulfill the request,
 * i.e. it must not be freed or destroyed until #blkdev_req.sem_done is
 * increased by the driver after the request is done.
 *
 * @warning
 * @a *req must be visible to the driver task, i.e. it must be either on the
 * kernel heap or in another memory region visible to the driver task.
 */
bool blkdev_enqueue_req(blkdev_req_t *req);

[[gnu::noreturn]]
void blkdev_task_entry(void);
