/**
 * @file blkdev.h
 * Block device worker task API.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ksemaphore.h"

typedef struct blkdev_req blkdev_req_t;

typedef struct {
    bool (*f_is_busy)(void *ctx);
    void (*f_submit_req)(blkdev_req_t *req);
} blkdev_if_t;

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

typedef struct {
    void *driver_ctx;
    blkdev_if_t driver_intf;
} blkdev_dev_t;

struct blkdev_req {
    _Atomic blkdev_req_state_t state;
    blkdev_op_t op;
    uint64_t start_sector;

    void *read_buf;
    size_t read_sectors;
    const void *write_buf;
    size_t write_sectors;

    blkdev_dev_t *dev;

    semaphore_t sem_done;
};

/**
 * Returns `true` if the blkdev task is ready to process requests.
 * See #blkdev_task_entry.
 */
bool blkdev_is_ready(void);

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

/**
 * Synchronously reads @a num_sectors starting from sector @a start_sector.
 *
 * @param dev          blkdev-level context of the device (see
 *                     #devmgr_dev_t.blkdev_dev).
 * @param start_sector First sector to read.
 * @param num_sectors  Number of sectors to read.
 * @param buf          Destination buffer.
 *
 * @returns `true` if @a num_sectors sectors starting from @a start_sector have
 * been read from the device @a dev and copied to @a buf.
 *
 * @note
 * This function is synchronous, i.e., it blocks the calling task until the read
 * request is finished.
 */
bool blkdev_sync_read(blkdev_dev_t *dev, uint64_t start_sector,
                      uint32_t num_sectors, void *buf);

[[gnu::noreturn]]
void blkdev_task_entry(void);
