#include "blkdev/blkpart.h"
#include "heap.h"
#include "kprintf.h"
#include "memfun.h"
#include "panic.h"

blkpart_ctx_t *blkpart_init(blkdev_dev_t *parent_dev, gpt_part_t *part) {
    blkpart_ctx_t *const blkpart_ctx = heap_alloc(sizeof(*blkpart_ctx));
    kmemset(blkpart_ctx, 0, sizeof(*blkpart_ctx));

    const uint64_t start_sector = part->starting_lba;
    const uint64_t num_sectors_u64 = part->ending_lba - part->starting_lba;
    if ((num_sectors_u64 >> 32) != 0) {
        panic_enter();
        kprintf("blkpart_init: partition is too big: 0x%08X%08X sectors\n",
                (uint32_t)(num_sectors_u64 >> 32), (uint32_t)num_sectors_u64);
        panic("blkpart_init failed");
    }

    blkpart_ctx->parent_dev = parent_dev;
    blkpart_ctx->start_sector = start_sector;
    blkpart_ctx->num_sectors = (uint32_t)num_sectors_u64;

    return blkpart_ctx;
}

void blkpart_fill_blkdev_if(blkdev_if_t *blkdev_if) {
    blkdev_if->f_is_busy = blkpart_if_is_busy;
    blkdev_if->f_submit_req = blkpart_if_submit_req;
}

bool blkpart_if_is_busy(void *v_blkpart_ctx) {
    blkpart_ctx_t *const blkpart_ctx = v_blkpart_ctx;
    return blkpart_ctx->parent_dev->driver_intf.f_is_busy(
        blkpart_ctx->parent_dev->driver_ctx);
}

void blkpart_if_submit_req(blkdev_req_t *req) {
    blkpart_ctx_t *const blkpart_ctx = req->dev->driver_ctx;
    blkpart_ctx->parent_dev->driver_intf.f_is_busy(req);
}
