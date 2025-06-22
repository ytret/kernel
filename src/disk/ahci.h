#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pci.h"

/**
 * Theoretical maximum number of ports per controller.
 * Refer to section 3.1.1, register CAP, field NP.
 */
#define AHCI_PORTS_PER_CTRL 32

typedef struct ahci_ctrl_ctx ahci_ctrl_ctx_t;
typedef struct ahci_port_ctx ahci_port_ctx_t;

ahci_ctrl_ctx_t *ahci_ctrl_new(const pci_dev_t *pci_dev);
ahci_port_ctx_t *ahci_ctrl_get_port(ahci_ctrl_ctx_t *ctrl_ctx, size_t port_idx);

bool ahci_port_is_online(const ahci_port_ctx_t *port_ctx);
const char *ahci_port_name(const ahci_port_ctx_t *port_ctx);
bool ahci_port_read(ahci_port_ctx_t *port_ctx, uint64_t start_sector,
                    uint32_t num_sectors, void *p_buf);
