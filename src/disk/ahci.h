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

typedef enum {
    AHCI_PORT_UNINIT,
    AHCI_PORT_IDLE,
    AHCI_PORT_READING,
} ahci_port_state_t;

ahci_ctrl_ctx_t *ahci_ctrl_new(const pci_dev_t *pci_dev);
ahci_port_ctx_t *ahci_ctrl_get_port(ahci_ctrl_ctx_t *ctrl_ctx, size_t port_idx);

bool ahci_port_is_online(const ahci_port_ctx_t *port_ctx);
const char *ahci_port_name(const ahci_port_ctx_t *port_ctx);

/**
 * Returns `true` if port @a port_ctx is ready for a new command.
 */
bool ahci_port_is_idle(ahci_port_ctx_t *port_ctx);

/**
 * Starts a read operation on port @a port_ctx.
 *
 * @param port_ctx     Port context pointer.
 * @param start_sector First sector to read.
 * @param num_sectors  Number of sectors to read.
 * @param out_buf      Output buffer.
 *
 * @returns
 * - `true` if the read command has been issued.
 * - `false` if the read command could not be issued, e.g., due to invalid
 *   sector parameters, or the port being busy.
 */
bool ahci_port_start_read(ahci_port_ctx_t *port_ctx, uint64_t start_sector,
                          uint32_t num_sectors, void *out_buf);
