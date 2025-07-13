#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "blkdev/blkdev.h"

typedef struct gpt_part gpt_part_t;

/// Kernel-level descriptor for GPT-partitioned disks.
typedef struct {
    gpt_part_t *parts;
    size_t num_parts;
} gpt_disk_t;

/// Kernel-level representation of a partition.
struct gpt_part {
    gpt_disk_t *disk;

    char *part_name;
    uint64_t starting_lba;
    uint64_t ending_lba;
    uint8_t type_guid[16];
    uint8_t part_guid[16];
};

/**
 * Returns `true` if a device *might* contain a GPT structure.
 * @param dev blkdev-level context of a device.
 * @note
 * This function only checks the GPT header signature. Parsing the GPT
 * structures may still fail.
 */
bool gpt_probe_signature(blkdev_dev_t *dev);

/**
 * Parses the GPT structures of a block device @a dev.
 *
 * @param[in]  dev          blkdev-level context of a device.
 * @param[out] out_gpt_disk Where to store #gpt_disk_t allocated by the
 *                          function.
 *
 * @returns `true` if @a dev is GPT-partitioned and @a *out_gpt_disk has been
 * written with GPT information (if it's not `NULL`). Otherwise, `false`.
 */
bool gpt_parse(blkdev_dev_t *dev, gpt_disk_t **out_gpt_disk);
