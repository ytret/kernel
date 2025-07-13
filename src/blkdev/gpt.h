#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

    /**
     * Partition used flag.
     * If #gpt_part.type_guid is all zeroes, then the partition is unused.
     */
    bool used;
};

/**
 * Returns `true` if a device *might* contain a GPT structure.
 * @param driver_ctx Driver context of a block device.
 * @note
 * This function only checks the GPT header signature. Parsing the GPT
 * structures may still fail.
 */
bool gpt_probe_signature(void *driver_ctx);

/**
 * Parses the GPT structures of a block device @a dev.
 *
 * @param[in]  driver_ctx   Driver context of a block device.
 * @param[out] out_gpt_disk Where to store #gpt_disk_t allocated by the
 *                          function.
 *
 * @returns `true` if @a dev is GPT-partitioned and @a *out_gpt_disk has been
 * written with GPT information (if it's not `NULL`). Otherwise, `false`.
 */
bool gpt_parse(void *driver_ctx, gpt_disk_t **out_gpt_disk);
