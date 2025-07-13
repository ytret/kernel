/**
 * @file blkpart.h
 * Block device partition driver.
 *
 * The blkpart driver is responsible for block-level access to a partition of a
 * block device. It translates any access to a partition to an access to the
 * underlying block device, which the partition is a part of.
 */

#pragma once

#include "blkdev/gpt.h"

typedef struct {
    blkdev_dev_t *parent_dev;
    uint64_t start_sector;
    uint32_t num_sectors;
} blkpart_ctx_t;

/**
 * Creates a new blkpart context for a partition of a block device.
 *
 * @warning
 * This function does not check the block device boundaries, i.e., if @a part is
 * outside of the block device, it will happily succeed.
 */
blkpart_ctx_t *blkpart_init(blkdev_dev_t *parent_dev, gpt_part_t *part);

/**
 * Fills the fields of the blkdev interface struct.
 * See #blkdev_if_t.
 */
void blkpart_fill_blkdev_if(blkdev_if_t *blkdev_if);

/**
 * Returns `true` if the parent block device of a partition is busy.
 *
 * Partitions of the same block device are represented as separate devices at
 * the kernel level. A partition can be considered busy if it itself has no
 * ongoing read or write operations, but one of its sibling partitions has.
 *
 * This is a part of the @ref blkdev_if_t "blkdev interface".
 */
bool blkpart_if_is_busy(void *v_blkpart_ctx);

/**
 * Passes a request to the underlying block device.
 * This is a part of the @ref blkdev_if_t "blkdev interface".
 */
void blkpart_if_submit_req(blkdev_req_t *req);
