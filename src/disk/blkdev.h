#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct blkdev_if blkdev_if_t;
typedef struct blkdev_req blkdev_req_t;

struct blkdev_if {
    bool (*f_is_busy)(void *ctx);
    void (*f_submit_req)(blkdev_req_t *req);
};

typedef enum {
    BLKDEV_OP_READ,
} blkdev_op_t;

struct blkdev_req {
    blkdev_op_t op;
    uint64_t start_sector;

    void *read_buf;
    size_t read_sectors;
    const void *write_buf;
    size_t write_sectors;

    void *dev_ctx;      //!< Device driver context for the interface.
    blkdev_if_t dev_if; //!< Device driver interface for blkdev.

    void (*cb_done)(bool success);
};

bool blkdev_enqueue_req(blkdev_req_t *req);

[[gnu::noreturn]]
void blkdev_task_entry(void);
