/**
 * @file devmgr.h
 * Device manager API.
 */

#pragma once

#include <stdint.h>

#include "blkdev/blkdev.h"
#include "blkdev/gpt.h"

typedef enum {
    DEVMGR_CLASS_NONE,
    DEVMGR_CLASS_BLOCK,
    DEVMGR_CLASS_BLOCK_PART,
} devmgr_class_t;

typedef enum {
    DEVMGR_DRVIER_NONE,
    DEVMGR_DRIVER_AHCI_PORT,
    DEVMGR_DRIVER_BLKPART,
} devmgr_driver_t;

typedef struct {
    uint32_t id;
    devmgr_class_t dev_class;
    devmgr_driver_t driver_id;

    /**
     * @{
     * @name Block-device-specific fields
     * These fields are valid only for block devices, e.g., #DEVMGR_CLASS_BLOCK.
     */
    blkdev_dev_t blkdev_dev;
    gpt_disk_t *gpt_disk;
    /// @}
} devmgr_dev_t;

/// Device iterator.
typedef struct {
    devmgr_class_t class_filter;
    uint32_t next_pos;
} devmgr_iter_t;

/// Enumerates devices and loads appropriate drivers.
void devmgr_init(void);

/**
 * Initializes partitions of block devices as separate devices.
 *
 * It should be called only when the @ref blkdev_task_entry "blkdev task" has
 * initialized its request queue. Otherwise, the GPT parser wouldn't be able to
 * read anything.
 */
void devmgr_init_blkdev_parts(void);

/**
 * Returns the device with ID @a id.
 * @param id Device identifier, see #devmgr_dev_t.id.
 * @returns
 * - Pointer to #devmgr_dev_t with the field #devmgr_dev_t.id equal to @a id.
 * - `NULL` if no such device exists.
 */
devmgr_dev_t *devmgr_get_by_id(uint32_t id);

/**
 * Finds the first device that has class @a dev_class.
 * @param dev_class Device class to search for.
 * @returns
 * - Pointer to the device representation, if a device has been found.
 * - `NULL` if no device with such class has been found.
 */
devmgr_dev_t *devmgr_find_by_class(devmgr_class_t dev_class);

/**
 * Initializes a device iterator.
 *
 * @param iter         Pointer to an (unitialized) iterator.
 * @param class_filter Device class to iterate through.
 *
 * Use #devmgr_iter_next() to get the next device from the iterator.
 *
 * If @a class_filter is not #DEVMGR_CLASS_NONE, then #devmgr_iter_next() will
 * return only those devices that have the class @a class_filter.
 */
void devmgr_iter_init(devmgr_iter_t *iter, devmgr_class_t class_filter);

/**
 * Gets the next device from an iterator.
 * @param iter Device iterator initialized with #devmgr_iter_init().
 * @returns
 * - Pointer to #devmgr_dev_t if there are devices left to iterate through.
 * - `NULL` otherwise.
 */
devmgr_dev_t *devmgr_iter_next(devmgr_iter_t *iter);

const char *devmgr_class_name(devmgr_class_t dev_class);
const char *devmgr_driver_name(devmgr_driver_t driver);
