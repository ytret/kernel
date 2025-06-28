/**
 * @file blkdev.c
 * Block device worker process.
 */

#include "disk/blkdev.h"
#include "kprintf.h"
#include "mutex.h"
#include "panic.h"
#include "queue.h"

#define blkdevMAX_REQS 32

typedef struct {
    task_mutex_t lock;

    /**
     * Request queue.
     * - Item type: const #blkdev_req_t.
     * - Max itemss: #blkdevMAX_REQS.
     */
    queue_t req_queue;
} blkdev_ctx_t;

static blkdev_ctx_t g_blkdev_ctx;

static void prv_blkdev_init(void);

bool blkdev_enqueue_req(blkdev_req_t *req) {
    // FIXME: ideally 'req' is const, but it cannot be const because
    // queue_write() can't work with const pointers.

    return queue_write(&g_blkdev_ctx.req_queue, req);
}

[[gnu::noreturn]]
void blkdev_task_entry(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    __asm__ volatile("sti");

    kprintf("blkdev init\n");

    prv_blkdev_init();

    for (;;) {
        blkdev_req_t req;
        queue_read(&g_blkdev_ctx.req_queue, &req, sizeof(blkdev_req_t));

        // Check the request.
        if (!req.dev_ctx) {
            kprintf("blkdev: bad request: dev_ctx = NULL\n");
            continue;
        }
        if (!req.dev_if) {
            kprintf("blkdev: bad request: dev_if = NULL\n");
            continue;
        }

        // Wait for the driver to be ready.
        // FIXME: currently the AHCI driver busy flag is updated in the idle
        // check function. Once it uses interrupts, we should use a semaphore
        // that is increased in the IRQ handler and decreased here.
        while (req.dev_if->f_is_busy) {}

        req.dev_if->f_submit(&req);
    }

    kprintf("blkdev_task_entry: reached end\n");
    panic("unexpected behavior");
}

static void prv_blkdev_init(void) {
    mutex_init(&g_blkdev_ctx.lock);
    queue_init(&g_blkdev_ctx.req_queue, blkdevMAX_REQS, sizeof(blkdev_req_t));
}
