#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ahci_regs.h"
#include "blkdev/blkdev.h"
#include "pci.h"

/**
 * Maximum number of supported ports per controller.
 *
 * Theoretically the maximum is 32 ports. However, (1) all 32 are rarely used,
 * and (2) QEMU maps two controllers 4096 bytes apart, meaning that not all 32
 * ports can be used anyway.
 *
 * One port-specific registers take 128 bytes. Per spec HBA memory size is
 * 0x1100 bytes, that's 0x100 (256 bytes or two ports) bytes larger than what is
 * used by QEMU.
 *
 * Refer to section 3.1.1, register CAP, field NP.
 */
#define AHCI_PORTS_PER_CTRL 30

/**
 * Interrupt number for the global AHCI interrupts.
 * IRQs of all AHCI controllers are mapped to this vector in the I/O APIC.
 */
#define AHCI_VEC_GLOBAL 0xA0

typedef struct ahci_ctrl_ctx ahci_ctrl_ctx_t;
typedef struct ahci_port_ctx ahci_port_ctx_t;

typedef enum {
    AHCI_PORT_UNINIT,
    AHCI_PORT_IDLE,
    AHCI_PORT_ACTIVE,
} ahci_port_state_t;

ahci_ctrl_ctx_t *ahci_ctrl_new(const pci_dev_t *pci_dev);
ahci_port_ctx_t *ahci_ctrl_get_port(ahci_ctrl_ctx_t *ctrl_ctx, size_t port_idx);

/**
 * Enables or disables the controller's global interrupt line.
 *
 * See #ahci_ghc_ghc_t.ie.
 *
 * @param ctrl_ctx Controller context pointer.
 * @param on       Enable (`true`) or disable (`false`) the global interrupt.
 */
void ahci_ctrl_set_int(ahci_ctrl_ctx_t *ctrl_ctx, bool on);

/**
 * Maps the IRQ of an AHCI Controller to a Local APIC vector.
 *
 * @param ctrl_ctx Controller context pointer.
 * @param vec      Interrupt vector to map the global IRQ of @a ctrl_ctx to.
 */
void ahci_ctrl_map_irq(ahci_ctrl_ctx_t *ctrl_ctx, uint8_t vec);

void ahci_ctrl_irq_handler(void);

bool ahci_port_is_online(const ahci_port_ctx_t *port_ctx);
const char *ahci_port_name(const ahci_port_ctx_t *port_ctx);

/**
 * Enables or disables an interrupt @a port_int on port @a port_ctx.
 *
 * @param port_ctx Port context pointer.
 * @param port_int Port interrupt selection.
 * @param on       Enable (`true`) or disable (`false`) interrupt @a port_int.
 */
void ahci_port_set_int(ahci_port_ctx_t *port_ctx, ahci_port_int_t port_int,
                       bool on);

void ahci_port_irq_handler(ahci_port_ctx_t *port_ctx);

/// Returns `true` if port @a port_ctx is ready for a new request.
bool ahci_port_is_idle(ahci_port_ctx_t *port_ctx);

/**
 * Starts a read operation on port @a port_ctx.
 *
 * @param port_ctx     Port context pointer.
 * @param start_sector First sector to read.
 * @param num_sectors  Number of sectors to read.
 * @param buf          Output buffer.
 *
 * @returns
 * - `true` if the read command has been issued.
 * - `false` if the read command could not be issued, e.g., due to invalid
 *   sector parameters, or the port being busy.
 */
bool ahci_port_start_read(ahci_port_ctx_t *port_ctx, uint64_t start_sector,
                          uint32_t num_sectors, void *buf);

/**
 * Starts a write operation on port @a port_ctx.
 *
 * @param port_ctx     Port context pointer.
 * @param start_sector First sector to write at.
 * @param num_sectors  Number of sectors to write.
 * @param buf          Source buffer.
 *
 * @returns
 * - `true` if the write command has been issued.
 * - `false` if the write command could not be issued, e.g., due to invalid
 *   sector parameters, or the port being busy.
 */
bool ahci_port_start_write(ahci_port_ctx_t *port_ctx, uint64_t start_sector,
                           uint32_t num_sectors, const void *buf);

/**
 * Fills the fields of the blkdev interface struct.
 * See #blkdev_if_t.
 */
void ahci_port_fill_blkdev_if(blkdev_if_t *blkdev_if);

/**
 * Returns `true` if port @a v_port_ctx is busy and cannot accept new requests.
 * This is a part of the @ref blkdev_if_t "blkdev interface".
 */
bool ahci_port_if_is_busy(void *v_port_ctx);

/**
 * Starts the processing of a blkdev request.
 * This is a part of the @ref blkdev_if_t "blkdev interface".
 */
void ahci_port_if_submit_req(blkdev_req_t *req);
