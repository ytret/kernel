/**
 * @file devdrv.h
 * Device manager API.
 */

#pragma once

typedef enum {
    DEVDRV_CLASS_NONE,
    DEVDRV_CLASS_DISK,
} devdrv_class_t;

typedef enum {
    DEVDRV_DRVIER_NONE,
    DEVDRV_DRIVER_AHCI_PORT,
} devdrv_driver_t;

typedef struct {
    devdrv_class_t dev_class;
    devdrv_driver_t driver_id;
    void *driver_ctx;
} devdrv_dev_t;

/// Enumerates devices and loads appropriate drivers.
void devdrv_init(void);

/**
 * Finds the first device that has class @a dev_class.
 * @param dev_class Device class to search for.
 * @returns
 * - Pointer to the device representation, if a device has been found.
 * - `NULL` if no device with such class has been found.
 */
devdrv_dev_t *devdrv_find_by_class(devdrv_class_t dev_class);
