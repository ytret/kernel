/**
 * @file devmgr.h
 * Device manager API.
 */

#pragma once

typedef enum {
    DEVMGR_CLASS_NONE,
    DEVMGR_CLASS_DISK,
} devmgr_class_t;

typedef enum {
    DEVMGR_DRVIER_NONE,
    DEVMGR_DRIVER_AHCI_PORT,
} devmgr_driver_t;

typedef struct {
    devmgr_class_t dev_class;
    devmgr_driver_t driver_id;
    void *driver_ctx;
} devmgr_dev_t;

/// Enumerates devices and loads appropriate drivers.
void devmgr_init(void);

/**
 * Finds the first device that has class @a dev_class.
 * @param dev_class Device class to search for.
 * @returns
 * - Pointer to the device representation, if a device has been found.
 * - `NULL` if no device with such class has been found.
 */
devmgr_dev_t *devmgr_find_by_class(devmgr_class_t dev_class);
