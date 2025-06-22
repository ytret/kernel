#pragma once

#include <stdint.h>

#include "devdrv.h"

typedef enum {
    DISK_READ,
} disk_iodir_t;

typedef struct {
    disk_iodir_t dir;
    void *read_buf;

    uint64_t start_sector;
    uint32_t num_sectors;
} disk_cmd_t;

bool disk_is_idle(devdrv_dev_t *dev);
bool disk_start_read(devdrv_dev_t *dev, uint64_t start_sector,
                     uint32_t num_sectors, void *p_buf);
