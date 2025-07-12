/**
 * @file blkdev.c
 * Block device worker process.
 */

#include "blkdev/blkdev.h"
#include "kprintf.h"
#include "mutex.h"
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

static void prv_blkdev_init(void);

bool blkdev_enqueue_req(blkdev_req_t *req) {
    return queue_write(&g_blkdev_ctx.req_queue, &req);
}

[[gnu::noreturn]]
void blkdev_task_entry(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    __asm__ volatile("sti");

    prv_blkdev_init();

    for (;;) {
        blkdev_req_t *req;
        queue_read(&g_blkdev_ctx.req_queue, &req, sizeof(uintptr_t));

        // Check the request.
        if (!req->dev_ctx) {
            kprintf("blkdev: bad request: dev_ctx = NULL\n");
            continue;
        }

        // Wait for the driver to be ready.
        // FIXME: if there are multiple blkdev drivers, one busy driver prevents
        // from other drivers getting their requests. Add per-block-device
        // workers with their own queues.
        while (req->dev_if.f_is_busy(req->dev_ctx)) {}

        req->dev_if.f_submit_req(req);
    }

    kprintf("blkdev: reached task end\n");
    panic("unexpected behavior");
}

static void prv_blkdev_init(void) {
    mutex_init(&g_blkdev_ctx.lock);
    queue_init(&g_blkdev_ctx.req_queue, blkdevMAX_REQS, sizeof(uintptr_t));
}
