#pragma once

#include <stdbool.h>
#include <stdint.h>

bool ahci_init(uint8_t bus, uint8_t dev);
bool ahci_read_sectors(uint64_t start_sector, uint32_t num_sectors,
                       void *p_buf);
bool ahci_is_init(void);
