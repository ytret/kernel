#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pci.h"

#define AHCI_PORTS_PER_CTRL 32

typedef struct ahci_ctrl_ctx ahci_ctrl_ctx_t;
typedef struct ahci_port_ctx ahci_port_ctx_t;

bool ahci_init(const pci_dev_t *pci_dev);

bool ahci_read_sectors(uint64_t start_sector, uint32_t num_sectors,
                       void *p_buf);
bool ahci_is_init(void);
