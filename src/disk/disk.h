#pragma once

#include <stdint.h>

#include "devdrv.h"

bool disk_read_sectors(devdrv_dev_t *dev, uint64_t start_sector,
                       uint32_t num_sectors, void *p_buf);
