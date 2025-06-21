#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pci.h"

bool ahci_init(const pci_dev_t *pci_dev);
bool ahci_read_sectors(uint64_t start_sector, uint32_t num_sectors,
                       void *p_buf);
bool ahci_is_init(void);
