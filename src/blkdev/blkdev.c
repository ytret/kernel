/**
 * @file blkdev.c
 * Block device worker process.
 */

#include "blkdev/blkdev.h"
#include "heap.h"
#include "kmutex.h"
#include "log.h"
#include "panic.h"
#include "queue.h"

#define blkdevMAX_REQS 32

typedef struct {
    task_mutex_t lock;

    /**
     * Request queue.
     * - Item type: pointer to #blkdev_req_t.
     * - Max itemss: #blkdevMAX_REQS.
     */
    queue_t req_queue;
} blkdev_ctx_t;

static blkdev_ctx_t g_blkdev_ctx;
static volatile _Atomic bool g_blkdev_ready;

static void prv_blkdev_init(void);

bool blkdev_is_ready(void) {
    return g_blkdev_ready;
}

bool blkdev_enqueue_req(blkdev_req_t *req) {
    return queue_write(&g_blkdev_ctx.req_queue, &req);
}

bool blkdev_sync_read(blkdev_dev_t *dev, uint64_t start_sector,
                      uint32_t num_sectors, void *buf) {
    bool ret;
    blkdev_req_t *const req = heap_alloc(sizeof(*req));

    req->state = BLKDEV_REQ_INACTIVE;
    req->op = BLKDEV_OP_READ;
    req->start_sector = start_sector;
    req->read_sectors = num_sectors;
    req->read_buf = buf;
    req->dev = dev;
    semaphore_init(&req->sem_done);

    if (!blkdev_enqueue_req(req)) {
        LOG_ERROR("failed to enqueue a request");
        heap_free(req);
        return false;
    }

    semaphore_decrease(&req->sem_done);

    ret = req->state == BLKDEV_REQ_SUCCESS;
    heap_free(req);

    return ret;
}

[[gnu::noreturn]]
void blkdev_task_entry(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    __asm__ volatile("sti");

    prv_blkdev_init();
    g_blkdev_ready = true;

    for (;;) {
        blkdev_req_t *req;
        queue_read(&g_blkdev_ctx.req_queue, &req, sizeof(uintptr_t));

        // Check the request.
        // FIXME: increase semaphore with error field set?
        if (!req->dev) {
            LOG_ERROR("bad request: dev = NULL");
            continue;
        }
        if (!req->dev->driver_intf.f_is_busy) {
            LOG_ERROR("bad request: dev->driver_intf.f_is_busy = NULL");
            continue;
        }
        if (!req->dev->driver_intf.f_submit_req) {
            LOG_ERROR("bad request: dev->driver_intf.f_submit_req = NULL");
            continue;
        }

        // Wait for the driver to be ready.
        // FIXME: if there are multiple blkdev drivers, one busy driver prevents
        // from other drivers getting their requests. Add per-block-device
        // workers with their own queues.
        while (req->dev->driver_intf.f_is_busy(req->dev->driver_ctx)) {}

        req->dev->driver_intf.f_submit_req(req);
    }

    LOG_ERROR("reached task end");
    panic("unexpected behavior");
}

static void prv_blkdev_init(void) {
    mutex_init(&g_blkdev_ctx.lock);
    queue_init(&g_blkdev_ctx.req_queue, blkdevMAX_REQS, sizeof(uintptr_t));
}
