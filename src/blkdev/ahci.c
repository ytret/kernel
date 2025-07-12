/**
 * @file ahci.c
 * SATA AHCI driver implementation.
 *
 * Refer to Serial ATA Advanced Host Controller Interface (AHCI) 1.3.1.
 */

#include <stddef.h>

#include "acpi/ioapic.h"
#include "acpi/lapic.h"
#include "assert.h"
#include "blkdev/ahci.h"
#include "blkdev/ahci_regs.h"
#include "devmgr.h"
#include "heap.h"
#include "kprintf.h"
#include "memfun.h"
#include "pci.h"
#include "string.h"
#include "vmm.h"

/**
 * ABAR register, base address mask.
 *
 * Although AHCI 1.3.1, section 2.1.11 (Offset 24h: ABAR - AHCI Base Address)
 * specifies bits 12..04 as reserved, QEMU uses the 12th bit. Maybe the whole
 * reserved field is meant to be used as an address, but that may take away its
 * page alignment property, which the code relies on.
 */
#define AHCI_ABAR_ADDR_MASK (~0xFFF)

/**
 * Maximum AHCI Controller name string size (bytes).
 *
 * Format: `ahcib<bus>d<device>f<function>`.
 * - `ahcib` -- 5 characters.
 * - `b<bus>` -- maximum value is determined by #PCI_ENUM_BUSES (3 characters).
 * - `d<device>` -- at maximum 32 devices per bus (3 characters).
 * - `f<function>` -- at maximum 8 functions per device (2 character).
 * - `NUL` byte at the end (1 byte).
 *
 * See #ahci_ctrl_ctx_t.name.
 */
static_assert(PCI_ENUM_BUSES <= 99, "increase AHCI_CTRL_NAME_SIZE");
static_assert(PCI_DEVS_PER_BUS <= 99, "increase AHCI_CTRL_NAME_SIZE");
static_assert(PCI_FUNS_PER_DEV <= 9, "increase AHCI_CTRL_NAME_SIZE");
#define AHCI_CTRL_NAME_SIZE (5 + 3 + 3 + 2 + 1)

/**
 * Maximum AHCI Port name string size (bytes).
 *
 * Format: `<controller>p<port number>`. Maximum port number is 32, so the
 * longest possible name is longer than the controller's longest name plus 3
 * characters.
 *
 * See #ahci_port_ctx_t.name.
 */
static_assert(AHCI_PORTS_PER_CTRL <= 99, "increase AHCI_PORT_NAME_SIZE");
#define AHCI_PORT_NAME_SIZE (AHCI_CTRL_NAME_SIZE + 3)

/**
 * AHCI Port context.
 */
struct ahci_port_ctx {
    /**
     * Unique kernel name for the device.
     * See #prv_ahci_set_port_name().
     */
    char name[AHCI_PORT_NAME_SIZE];

    /**
     * Device serial string.
     * It is available after the device has been identified, see
     * #ahci_port_ctx.identified.
     */
    char serial_str[SATA_SERIAL_STR_LEN + 1];

    /// Port index in the controller #ahci_port_ctx.ctrl_ctx.
    size_t port_num;
    /// Port is online and is a SATA device.
    bool online_sata;
    /// Port parameters have been identified using #SATA_CMD_IDENTIFY_DEVICE.
    bool identified;

    _Atomic ahci_port_state_t state;
    blkdev_req_t *blkdev_req;

    /// Context pointer of the controller this port is a part of.
    ahci_ctrl_ctx_t *ctrl_ctx;
    /// Pointer to the Port register.
    reg_port_t *reg_port;
    /**
     * Number of sectors in the device.
     * This value is available only after the device has been identified, see
     * #ahci_port_ctx.identified.
     */
    uint32_t num_sectors;

    /**
     * Received FIS buffer.
     * It is allocated on the heap during the port setup process, see
     * #prv_ahci_setup_port(). Refer to section 4.2.1, Received FIS Structure.
     */
    ahci_rfis_t *p_rfis;
    /**
     * Command List.
     * It is allocated on the heap during the port setup process, see
     * #prv_ahci_setup_port().
     * Refer to section 4.2.2, Command List Structure.
     */
    ahci_cmd_hdr_t *p_cmd_list;
    /**
     * Command Table array.
     * It is allocated on the heap during the port setup process, see
     * #prv_ahci_setup_port().
     * Refer to section 4.2.3, Command Table.
     */
    ahci_cmd_table_t *p_cmd_tables;
};

/**
 * AHCI Controller context.
 */
struct ahci_ctrl_ctx {
    /**
     * Unique kernel name for the device.
     * See #prv_ahci_set_ctrl_name().
     */
    char name[AHCI_CTRL_NAME_SIZE];

    /// PCI device context.
    const pci_dev_t *pci_dev;

    /**
     * IRQ number extracted from the PCI header.
     * See #pci_header_00h_t.int_line;
     */
    uint8_t irq;

    /// Generic HBA Control register.
    reg_ghc_t *reg_ghc;

    /**
     * Port contexts.
     *
     * The controller manifests its number of ports in its HBA Capability
     * register, field NP (Number of Ports). However, the number of ports it
     * implements may be less than that and is specified in the PI (Ports
     * Implemented) register.
     *
     * Use #ahci_port_ctx.online_sata to distinguish online (with established
     * Phy communication) ports.
     */
    struct ahci_port_ctx ports[AHCI_PORTS_PER_CTRL];
};

/**
 * Internal representation of an ATA command.
 * The meaning of these fields depends on the specific ATA command that is used,
 * refer to the ATA/ATAPI Command Set specification.
 */
typedef struct {
    uint16_t features;
    uint16_t count;
    uint64_t lba;
    uint8_t device;
    uint8_t command;
} ata_cmd_t;

static void prv_ahci_set_ctrl_name(ahci_ctrl_ctx_t *ctrl_ctx);
static bool prv_ahci_enter_ahci_mode(ahci_ctrl_ctx_t *ctrl_ctx);
static void prv_ahci_enumerate_ports(ahci_ctrl_ctx_t *ctrl_ctx);

static void prv_ahci_set_port_name(ahci_port_ctx_t *port_ctx);
static void prv_ahci_setup_port(ahci_port_ctx_t *port_ctx);
static void prv_ahci_identify_port(ahci_port_ctx_t *port_ctx);

static bool send_read_cmd(ahci_port_ctx_t *port_ctx, ata_cmd_t cmd, void *p_buf,
                          size_t num_sectors, size_t *p_cmd_slot);
static bool wait_for_cmd(ahci_port_ctx_t *port_ctx, size_t cmd_slot);
static bool find_cmd_slot(reg_port_t *reg_port, size_t *out_slot);

ahci_ctrl_ctx_t *ahci_ctrl_new(const pci_dev_t *pci_dev) {
    const uint32_t abar = pci_dev->header.bar5;
    const uint32_t hba_regs_addr = abar & AHCI_ABAR_ADDR_MASK;

    for (uint32_t page = hba_regs_addr;
         page < hba_regs_addr + AHCI_HBA_MAP_SIZE; page += 4096) {
        vmm_map_kernel_page(page, page);
    }

    ahci_ctrl_ctx_t *ctrl_ctx =
        heap_alloc_aligned(sizeof(ahci_ctrl_ctx_t), alignof(ahci_ctrl_ctx_t));
    kmemset(ctrl_ctx, 0, sizeof(*ctrl_ctx));
    ctrl_ctx->pci_dev = pci_dev;
    ctrl_ctx->reg_ghc = (reg_ghc_t *)hba_regs_addr;

    prv_ahci_set_ctrl_name(ctrl_ctx);

    if (!prv_ahci_enter_ahci_mode(ctrl_ctx)) {
        heap_free(ctrl_ctx);
        return NULL;
    }

    prv_ahci_enumerate_ports(ctrl_ctx);
    for (size_t port_idx = 0; port_idx < AHCI_PORTS_PER_CTRL; port_idx++) {
        ahci_port_ctx_t *const port_ctx = &ctrl_ctx->ports[port_idx];
        if (port_ctx->online_sata) {
            prv_ahci_setup_port(port_ctx);
            prv_ahci_identify_port(port_ctx);
            port_ctx->state = AHCI_PORT_IDLE;
        }
    }

    ctrl_ctx->irq = pci_dev->header.int_line;

    return ctrl_ctx;
}

ahci_port_ctx_t *ahci_ctrl_get_port(ahci_ctrl_ctx_t *ctrl_ctx,
                                    size_t port_idx) {
    if (port_idx < AHCI_PORTS_PER_CTRL) {
        return &ctrl_ctx->ports[port_idx];
    } else {
        return NULL;
    }
}

void ahci_ctrl_set_int(ahci_ctrl_ctx_t *ctrl_ctx, bool on) {
    ahci_ghc_ghc_t ctrl_ghc;
    kmemread_v4(&ctrl_ghc, &ctrl_ctx->reg_ghc->ghc);
    ctrl_ghc.ie = on;
    kmemwrite_v4(&ctrl_ctx->reg_ghc->ghc, &ctrl_ghc);
}

void ahci_ctrl_map_irq(ahci_ctrl_ctx_t *ctrl_ctx, uint8_t vec) {
    ioapic_map_irq(ctrl_ctx->irq, vec, lapic_get_id());
}

void ahci_ctrl_irq_handler(void) {
    devmgr_iter_t iter;
    devmgr_iter_init(&iter, DEVMGR_CLASS_DISK);

    devmgr_dev_t *dev;
    while ((dev = devmgr_iter_next(&iter))) {
        if (dev->driver_id == DEVMGR_DRIVER_AHCI_PORT) {
            ahci_port_ctx_t *const port_ctx = dev->driver_ctx;
            ahci_port_irq_handler(port_ctx);
        }
    }

    lapic_send_eoi();
}

bool ahci_port_is_online(const ahci_port_ctx_t *port_ctx) {
    return port_ctx->online_sata;
}

const char *ahci_port_name(const ahci_port_ctx_t *port_ctx) {
    return port_ctx->name;
}

void ahci_port_set_int(ahci_port_ctx_t *port_ctx, ahci_port_int_t port_int,
                       bool on) {
    uint32_t ie_val = port_ctx->reg_port->ie;
    if (on) {
        ie_val |= (uint32_t)port_int;
    } else {
        ie_val &= ~((uint32_t)port_int);
    }
    port_ctx->reg_port->ie = ie_val;
}

void ahci_port_irq_handler(ahci_port_ctx_t *port_ctx) {
    const uint32_t int_status = port_ctx->reg_port->is;

    if (int_status & AHCI_PORT_INT_DHR) {
        port_ctx->reg_port->is = AHCI_PORT_INT_DHR;
        kprintf("ahci port interrupt: AHCI_PORT_INT_DHR\n");

        const ahci_port_state_t port_state = port_ctx->state;
        if (port_state == AHCI_PORT_READING) {
            kprintf("ahci port irq: AHCI_PORT_READING\n");
            if (port_ctx->blkdev_req->state == BLKDEV_REQ_ACTIVE) {
                kprintf("ahci port irq: active req\n");
                port_ctx->blkdev_req->state = BLKDEV_REQ_SUCCESS;
                semaphore_increase(&port_ctx->blkdev_req->sem_done);
            } else {
                panic_enter();
                kprintf("ahci_port_irq_handler: port %s state is "
                        "AHCI_PORT_READING, but there is no active request\n",
                        port_ctx->name);
                panic("unexpected AHCI DHR IRQ");
            }
            port_ctx->state = AHCI_PORT_IDLE;
        } else {
            panic_enter();
            kprintf("ahci_port_irq_handler: unexpected port %s state: %u\n",
                    port_ctx->name, port_state);
            panic("unexpected AHCI DHR IRQ");
        }
    }
    if (int_status & AHCI_PORT_INT_PS) {
        port_ctx->reg_port->is = AHCI_PORT_INT_PS;
        kprintf("ahci port interrupt: AHCI_PORT_INT_PS\n");
    }
    if (int_status & AHCI_PORT_INT_DS) {
        port_ctx->reg_port->is = AHCI_PORT_INT_DS;
        kprintf("ahci port interrupt: AHCI_PORT_INT_DS\n");
    }
    if (int_status & AHCI_PORT_INT_SDB) {
        port_ctx->reg_port->is = AHCI_PORT_INT_SDB;
        kprintf("ahci port interrupt: AHCI_PORT_INT_SDB\n");
    }
    if (int_status & AHCI_PORT_INT_UF) {
        port_ctx->reg_port->is = AHCI_PORT_INT_UF;
        kprintf("ahci port interrupt: AHCI_PORT_INT_UF\n");
    }
    if (int_status & AHCI_PORT_INT_DP) {
        port_ctx->reg_port->is = AHCI_PORT_INT_DP;
        kprintf("ahci port interrupt: AHCI_PORT_INT_DP\n");
    }
    if (int_status & AHCI_PORT_INT_PC) {
        port_ctx->reg_port->is = AHCI_PORT_INT_PC;
        kprintf("ahci port interrupt: AHCI_PORT_INT_PC\n");
    }
    if (int_status & AHCI_PORT_INT_DMP) {
        port_ctx->reg_port->is = AHCI_PORT_INT_DMP;
        kprintf("ahci port interrupt: AHCI_PORT_INT_DMP\n");
    }
    if (int_status & AHCI_PORT_INT_PRC) {
        port_ctx->reg_port->is = AHCI_PORT_INT_PRC;
        kprintf("ahci port interrupt: AHCI_PORT_INT_PRC\n");
    }
    if (int_status & AHCI_PORT_INT_IPM) {
        port_ctx->reg_port->is = AHCI_PORT_INT_IPM;
        kprintf("ahci port interrupt: AHCI_PORT_INT_IPM\n");
    }
    if (int_status & AHCI_PORT_INT_OF) {
        port_ctx->reg_port->is = AHCI_PORT_INT_OF;
        kprintf("ahci port interrupt: AHCI_PORT_INT_OF\n");
    }
    if (int_status & AHCI_PORT_INT_INF) {
        port_ctx->reg_port->is = AHCI_PORT_INT_INF;
        kprintf("ahci port interrupt: AHCI_PORT_INT_INF\n");
    }
    if (int_status & AHCI_PORT_INT_IF) {
        port_ctx->reg_port->is = AHCI_PORT_INT_IF;
        kprintf("ahci port interrupt: AHCI_PORT_INT_IF\n");
    }
    if (int_status & AHCI_PORT_INT_HBD) {
        port_ctx->reg_port->is = AHCI_PORT_INT_HBD;
        kprintf("ahci port interrupt: AHCI_PORT_INT_HBD\n");
    }
    if (int_status & AHCI_PORT_INT_HBF) {
        port_ctx->reg_port->is = AHCI_PORT_INT_HBF;
        kprintf("ahci port interrupt: AHCI_PORT_INT_HBF\n");
    }
    if (int_status & AHCI_PORT_INT_TFE) {
        port_ctx->reg_port->is = AHCI_PORT_INT_TFE;
        kprintf("ahci port interrupt: AHCI_PORT_INT_TFE\n");
    }
    if (int_status & AHCI_PORT_INT_CPD) {
        port_ctx->reg_port->is = AHCI_PORT_INT_CPD;
        kprintf("ahci port interrupt: AHCI_PORT_INT_CPD\n");
    }
}

bool ahci_port_is_idle(ahci_port_ctx_t *port_ctx) {
    return port_ctx->state == AHCI_PORT_IDLE;
}

bool ahci_port_start_read(ahci_port_ctx_t *port_ctx, uint64_t start_sector,
                          uint32_t num_sectors, void *p_buf) {
    if (port_ctx->state != AHCI_PORT_IDLE) { return false; }

    // Check start_sector.
    if (start_sector >> 48) {
        kprintf("ahci: %s: start sector number cannot be wider than 48 bits\n",
                port_ctx->name);
        return false;
    }
    if (start_sector >= port_ctx->num_sectors) {
        kprintf("ahci: %s: start sector is past disk end by %u sectors\n",
                port_ctx->name, start_sector - port_ctx->num_sectors);
        return false;
    }

    // Check num_sectors.
    if (0 == num_sectors) { return true; }
    if (start_sector + num_sectors > port_ctx->num_sectors) {
        kprintf("ahci: %s: cannot read past disk end by %u sectors\n",
                port_ctx->name,
                (start_sector + num_sectors) - port_ctx->num_sectors);
        return false;
    }

    // We have a finite amount of PRDs. Each can describe a 4 MiB data block,
    // or 8192 512-byte sectors.
    if (num_sectors > 8192 * AHCI_CMD_TABLE_NUM_PRDS) {
        kprintf(
            "ahci: %s: number of sectors to read cannot be greater than %u\n",
            port_ctx->name, 8192 * AHCI_CMD_TABLE_NUM_PRDS);
        return false;
    }

    // Set up an ATA command.
    ata_cmd_t cmd = {0};
    cmd.count = num_sectors;
    cmd.lba = start_sector;
    cmd.device = (1 << 6);
    cmd.command = SATA_CMD_READ_DMA_EXT;

    // Issue the command.
    port_ctx->state = AHCI_PORT_READING;
    size_t cmd_slot;
    if (!send_read_cmd(port_ctx, cmd, p_buf, num_sectors, &cmd_slot)) {
        port_ctx->state = AHCI_PORT_IDLE;
        kprintf("ahci: %s: failed to issue read command\n", port_ctx->name);
        return false;
    }

    return true;
}

void ahci_port_fill_blkdev_if(blkdev_if_t *blkdev_if) {
    blkdev_if->f_is_busy = ahci_port_if_is_busy;
    blkdev_if->f_submit_req = ahci_port_if_submit_req;
}

bool ahci_port_if_is_busy(void *v_port_ctx) {
    ahci_port_ctx_t *port_ctx = v_port_ctx;
    return !ahci_port_is_idle(port_ctx);
}

void ahci_port_if_submit_req(blkdev_req_t *req) {
    ahci_port_ctx_t *port_ctx = req->dev_ctx;

    switch (req->op) {
    case BLKDEV_OP_READ:
        if (ahci_port_is_idle(req->dev_ctx)) {
            port_ctx->blkdev_req = req;
            port_ctx->blkdev_req->state = BLKDEV_REQ_ACTIVE;
            if (!ahci_port_start_read(req->dev_ctx, req->start_sector,
                                      req->read_sectors, req->read_buf)) {
                port_ctx->blkdev_req->state = BLKDEV_REQ_ERROR;
                semaphore_increase(&req->sem_done);
            }
        } else {
            req->state = BLKDEV_REQ_ERROR;
            semaphore_increase(&req->sem_done);
        }
        break;
    }
}

static void prv_ahci_set_ctrl_name(ahci_ctrl_ctx_t *ctrl_ctx) {
    ASSERT(ctrl_ctx->pci_dev);

    const char prefix[] = "ahci";
    const char prefix_bus[] = "b";
    const char prefix_dev[] = "d";
    const char prefix_fun[] = "f";

    size_t pos = 0;
    char max_u32_str[11] = {0};

    // Name prefix.
    ASSERT(pos + sizeof(prefix) <= AHCI_CTRL_NAME_SIZE);
    kmemcpy(&ctrl_ctx->name[pos], prefix, sizeof(prefix));
    pos += sizeof(prefix) - 1; // do not count the NUL byte

    // Bus prefix.
    ASSERT(pos + sizeof(prefix_bus) <= AHCI_CTRL_NAME_SIZE);
    kmemcpy(&ctrl_ctx->name[pos], prefix_bus, sizeof(prefix_bus));
    pos += sizeof(prefix_bus) - 1;

    // Bus number.
    const size_t bus_len =
        string_itoa(ctrl_ctx->pci_dev->bus_num, false, max_u32_str, 10);
    ASSERT(bus_len > 0);
    ASSERT(pos + bus_len <= AHCI_CTRL_NAME_SIZE);
    kmemcpy(&ctrl_ctx->name[pos], max_u32_str, bus_len);
    pos += bus_len - 1;

    // Device prefix.
    ASSERT(pos + sizeof(prefix_dev) <= AHCI_CTRL_NAME_SIZE);
    kmemcpy(&ctrl_ctx->name[pos], prefix_dev, sizeof(prefix_dev));
    pos += sizeof(prefix_dev) - 1;

    // Device number.
    const size_t dev_len =
        string_itoa(ctrl_ctx->pci_dev->dev_num, false, max_u32_str, 10);
    ASSERT(dev_len > 0);
    ASSERT(pos + dev_len <= AHCI_CTRL_NAME_SIZE);
    kmemcpy(&ctrl_ctx->name[pos], max_u32_str, dev_len);
    pos += dev_len - 1; // do not count the NUL byte

    // Function prefix.
    ASSERT(pos + sizeof(prefix_fun) <= AHCI_CTRL_NAME_SIZE);
    kmemcpy(&ctrl_ctx->name[pos], prefix_fun, sizeof(prefix_fun));
    pos += sizeof(prefix_fun) - 1;

    // Function number.
    const size_t fun_len =
        string_itoa(ctrl_ctx->pci_dev->fun_num, false, max_u32_str, 10);
    ASSERT(fun_len > 0);
    ASSERT(pos + fun_len <= AHCI_CTRL_NAME_SIZE);
    kmemcpy(&ctrl_ctx->name[pos], max_u32_str, fun_len);
    pos += fun_len - 1; // do not count the NUL byte

    ASSERT(pos < AHCI_CTRL_NAME_SIZE);
    ctrl_ctx->name[pos] = 0;
}

/**
 * Enables AHCI mode on @a ctrl_ctx.
 * @param ctrl_ctx AHCI Controller context.
 * @returns `true` if the controller is in or has been placed in AHCI mode,
 * otherwise returns `false`.
 */
static bool prv_ahci_enter_ahci_mode(ahci_ctrl_ctx_t *ctrl_ctx) {
    reg_ghc_t *const p_ghc = ctrl_ctx->reg_ghc;

    if (p_ghc->ghc & AHCI_GHC_GHC_AE) {
        // AE set means AHCI mode is enabled.
        return true;
    }

    if (p_ghc->cap & AHCI_GHC_CAP_SAM) {
        // SAM set means the SATA controller supports both AHCI and legacy
        // interfaces. Enable AHCI.
        uint32_t ghc_val = p_ghc->ghc;
        ghc_val |= AHCI_GHC_GHC_AE;
        p_ghc->ghc = ghc_val;

        // Ensure that AE was set.
        if (p_ghc->ghc & AHCI_GHC_GHC_AE) {
            return true;
        } else {
            kprintf("ahci: cannot set GHC.AE bit, when it must be R/W"
                    " because CAP.SAM is set\n");
            return false;
        }
    } else {
        // SAM is reset, so the controller supports only AHCI mode.  However,
        // AE is reset, meaning that AHCI mode is disabled.
        kprintf("ahci: CAP.SAM is reset, meaning that SATA controller supports "
                "only AHCI mode; but GHC.AE is reset, meaning that AHCI mode "
                "is disabled\n");
        return false;
    }
}

/**
 * Enumerates ports of the @a ctrl_ctx controller.
 *
 * Initializes the following fields of each port context:
 * - #ahci_port_ctx_t.name
 * - #ahci_port_ctx_t.port_num
 * - #ahci_port_ctx_t.online_sata
 * - #ahci_port_ctx_t.state
 * - #ahci_port_ctx_t.ctrl_ctx
 * - #ahci_port_ctx_t.reg_port
 */
static void prv_ahci_enumerate_ports(ahci_ctrl_ctx_t *ctrl_ctx) {
    reg_ghc_t *const reg_ghc = ctrl_ctx->reg_ghc;

    ahci_cap_t ctrl_cap;
    kmemread_v4(&ctrl_cap, &reg_ghc->cap);

    kprintf("ahci: %s: port cabaility: %u\n", ctrl_ctx->name, ctrl_cap.np + 1);
    kprintf("ahci: %s: implemented ports: 0x%08X\n", ctrl_ctx->name,
            reg_ghc->pi);

    for (size_t port_idx = 0; port_idx < AHCI_PORTS_PER_CTRL; port_idx++) {
        ahci_port_ctx_t *const port_ctx = &ctrl_ctx->ports[port_idx];
        kmemset(port_ctx, 0, sizeof(*port_ctx));

        port_ctx->port_num = port_idx;
        port_ctx->online_sata = false;
        port_ctx->state = AHCI_PORT_UNINIT;
        port_ctx->ctrl_ctx = ctrl_ctx;
        port_ctx->reg_port = AHCI_REG_PORT(reg_ghc, port_idx);
        prv_ahci_set_port_name(port_ctx);

        const bool impl = (reg_ghc->pi & (1 << port_idx)) != 0;
        if (!impl) { continue; }

        reg_port_t *const reg_port = port_ctx->reg_port;
        ahci_port_ssts_t port_ssts;
        kmemread_v4(&port_ssts, &reg_port->ssts);
        switch (port_ssts.det) {
        case AHCI_SSTS_DET_NDEV_NPHY:
            kprintf("ahci: %s: no device\n", port_ctx->name);
            break;
        case AHCI_SSTS_DET_DEV_NPHY:
            kprintf("ahci: %s: has device, no Phy communication\n",
                    port_ctx->name);
            break;
        case AHCI_SSTS_DET_DEV_PHY:
            if (port_ctx->reg_port->sig == SATA_SIG_ATA) {
                port_ctx->online_sata = true;
                kprintf("ahci: %s: detected SATA_SIG_ATA\n", port_ctx->name);
            } else {
                kprintf("ahci: %s: unrecognized signature 0x%08X\n",
                        port_ctx->name);
            }
            break;
        case AHCI_SSTS_DET_PHY_OFF:
            kprintf("ahci: port %u: has device, Phy offline \n",
                    port_ctx->name);
            break;
        }
    }
}

static void prv_ahci_set_port_name(ahci_port_ctx_t *port_ctx) {
    const char prefix_port[] = "p";

    const size_t ctrl_name_len = string_len(port_ctx->ctrl_ctx->name);
    size_t pos = ctrl_name_len;
    char max_u32_str[11] = {0};

    static_assert(AHCI_PORT_NAME_SIZE >= AHCI_CTRL_NAME_SIZE + 3);
    kmemcpy(port_ctx->name, port_ctx->ctrl_ctx->name, ctrl_name_len);

    // Port prefix.
    ASSERT(pos + sizeof(prefix_port) <= AHCI_CTRL_NAME_SIZE);
    kmemcpy(&port_ctx->name[pos], prefix_port, sizeof(prefix_port));
    pos += sizeof(prefix_port) - 1;

    // Port number.
    const size_t port_len =
        string_itoa(port_ctx->port_num, false, max_u32_str, 10);
    ASSERT(port_len > 0);
    ASSERT(pos + port_len <= AHCI_CTRL_NAME_SIZE);
    kmemcpy(&port_ctx->name[pos], max_u32_str, port_len);
    pos += port_len - 1;

    ASSERT(pos < AHCI_CTRL_NAME_SIZE);
    port_ctx->name[pos] = 0;
}

/**
 * Sets up port @a port_ctx for communication.
 *
 * - Allocates buffers.
 * - Initializes the port registers.
 */
static void prv_ahci_setup_port(ahci_port_ctx_t *port_ctx) {
    ASSERT(port_ctx);
    ASSERT(port_ctx->reg_port);
    reg_port_t *const reg_port = port_ctx->reg_port;

    port_ctx->p_rfis =
        heap_alloc_aligned(sizeof(*port_ctx->p_rfis), AHCI_FIS_BASE_ALIGN);
    port_ctx->p_cmd_list = heap_alloc_aligned(
        AHCI_CMD_LIST_LEN * sizeof(ahci_cmd_hdr_t), AHCI_CMD_LIST_ALIGN);
    port_ctx->p_cmd_tables =
        heap_alloc_aligned(AHCI_CMD_LIST_LEN * sizeof(ahci_cmd_table_t),
                           alignof(ahci_cmd_table_t));

    // Stop FIS receive and command list DMA engines. The order is important.
    // Refer to section 10.3, Software Manipulation of Port DMA Engines.
    reg_port->cmd &= ~AHCI_PORT_CMD_ST;
    reg_port->cmd &= ~AHCI_PORT_CMD_FRE;
    while ((reg_port->cmd & AHCI_PORT_CMD_FR) ||
           (reg_port->cmd & AHCI_PORT_CMD_CR)) {}

    // Initialzie the Command List Base Address and FIS Base Address registers.
    reg_port->clb = (uint32_t)port_ctx->p_cmd_list;
    reg_port->clbu = 0;
    reg_port->fb = (uint32_t)port_ctx->p_rfis;
    reg_port->fbu = 0;

    // Set up each command header to point to the appropriate command table.
    for (size_t cmd_idx = 0; cmd_idx < AHCI_CMD_LIST_LEN; cmd_idx++) {
        ahci_cmd_hdr_t *const p_cmd_hdr = &port_ctx->p_cmd_list[cmd_idx];
        p_cmd_hdr->ctba = (uint32_t)&port_ctx->p_cmd_tables[cmd_idx];
        p_cmd_hdr->ctbau = 0;
    }

    // Start the command list DMA engine. The order is important.
    reg_port->cmd |= AHCI_PORT_CMD_FRE;
    reg_port->cmd |= AHCI_PORT_CMD_ST;

    // Wait for them to start.
    while (((reg_port->cmd & AHCI_PORT_CMD_CR) == 0) ||
           ((reg_port->cmd & AHCI_PORT_CMD_FR) == 0)) {}
}

/**
 * Sends a #SATA_CMD_IDENTIFY_DEVICE command on @a port_ctx.
 *
 * Fills in the following fields of @a port_ctx:
 * - #ahci_port_ctx_t.serial_str
 * - #ahci_port_ctx_t.identified
 * - #ahci_port_ctx_t.num_sectors
 */
static void prv_ahci_identify_port(ahci_port_ctx_t *port_ctx) {
    ASSERT(port_ctx);
    ASSERT(port_ctx->reg_port);

    ata_cmd_t cmd = {0};
    cmd.command = SATA_CMD_IDENTIFY_DEVICE;

    uint16_t *const p_ident = heap_alloc_aligned(512, 2);
    size_t cmd_slot;
    if (!send_read_cmd(port_ctx, cmd, p_ident, 1, &cmd_slot)) {
        kprintf("ahci: %s: could not issue IDENTIFY_DEVICE\n", port_ctx->name);
        heap_free(p_ident);
        port_ctx->identified = false;
        return;
    }

    bool b_ok = wait_for_cmd(port_ctx, cmd_slot);
    if (!b_ok) {
        kprintf("ahci: %s: command IDENTIFY_DEVICE failed\n", port_ctx->name);
        heap_free(p_ident);
        port_ctx->identified = false;
        return;
    }

    // Clear all interrupt status flags.
    port_ctx->reg_port->is = AHCI_PORT_INT_ALL;

    kmemcpy(port_ctx->serial_str, p_ident + 10, SATA_SERIAL_STR_LEN);
    port_ctx->num_sectors = *((uint32_t *)&p_ident[60]);

    kprintf("ahci: %s: serial is '%s'\n", port_ctx->name, port_ctx->serial_str);
    kprintf("ahci: %s: number of sectors: %u\n", port_ctx->name,
            port_ctx->num_sectors);

    heap_free(p_ident);
    port_ctx->identified = true;
}

/**
 * Sends an ATA command to read from a device.
 *
 * @param[in]  port_ctx    Context of the port that will handle the command.
 * @param[in]  cmd         ATA command details.
 * @param[in]  p_buf       Output buffer to copy the data read from the device.
 * @param[in]  num_sectors Number of sectors to be received into @a p_buf.
 * @param[out] p_cmd_slot  Command slot pointer.
 *
 * If the command has been issued, writes its slot to @a p_cmd_slot.
 *
 * @returns `true` if the command has been issued, otherwise `false`.
 */
static bool send_read_cmd(ahci_port_ctx_t *port_ctx, ata_cmd_t cmd, void *p_buf,
                          size_t num_sectors, size_t *p_cmd_slot) {
    // Depending on the sector count, there may be one or more physical region
    // descriptors necessary. Maximum region length is 4 MiB. However, the last
    // region length may be less than that to ensure that the buffer is not
    // overwritten.

    if (num_sectors == 0) {
        // FIXME: do not fail
        panic_enter();
        kprintf("send_read_cmd: port %s, num_sectors = 0\n", port_ctx->name);
        panic("invalid send_read_cmd argument");
    }

    // One PRD can describe a 4 MiB block of data.
    const size_t read_size = 512 * num_sectors;
    const size_t num_prds = (read_size + 0x3FFFFF) / 0x400000;
    const size_t last_prd_len = read_size % 0x400000;

    if (num_prds > AHCI_CMD_TABLE_NUM_PRDS) {
        kprintf("ahci: %s: not enough PRDs to transfer %u bytes\n",
                port_ctx->name, read_size);
        return false;
    }

    // Find a free command slot.
    size_t cmd_slot;
    if (!find_cmd_slot(port_ctx->reg_port, &cmd_slot)) {
        kprintf("ahci: %s: could not find free command slot\n", port_ctx->name);
        return false;
    }

    // Set up the command header.
    ahci_cmd_hdr_t *const p_cmd_hdr = &port_ctx->p_cmd_list[cmd_slot];
    p_cmd_hdr->cfl = (sizeof(sata_fis_reg_h2d_t) / 4);
    p_cmd_hdr->b_atapi = false;
    p_cmd_hdr->b_write = false;
    p_cmd_hdr->b_prefetch = false;
    p_cmd_hdr->b_reset = false;
    p_cmd_hdr->b_bist = 0;
    p_cmd_hdr->b_clear_bsy = false;
    p_cmd_hdr->pmp = 0;
    p_cmd_hdr->prdtl = num_prds;

    // Fill the command table. Start with the PRD table.
    ahci_cmd_table_t *const p_cmd_table = &port_ctx->p_cmd_tables[cmd_slot];
    ASSERT(num_prds >= 1);
    for (size_t prd_idx = 0; prd_idx < num_prds - 1; prd_idx++) {
        ahci_prd_t *const p_prd = &p_cmd_table->p_prd_table[prd_idx];
        p_prd->dba = (uint32_t)p_buf + 0x400000 * prd_idx;
        p_prd->dbau = 0;
        p_prd->dbc = 0x3FFFFF; // 4 MiB
        p_prd->b_int = true;
    }
    // The last PRD may describe less than 4 MiB, that's why it's not in the
    // loop.
    ahci_prd_t *const last_prd = &p_cmd_table->p_prd_table[num_prds - 1];
    last_prd->dba = (uint32_t)p_buf + 0x400000 * (num_prds - 1);
    last_prd->dbau = 0;
    last_prd->dbc = last_prd_len;
    last_prd->b_int = true;

    // Finish by setting the CFIS.
    sata_fis_reg_h2d_t *p_cfis = (sata_fis_reg_h2d_t *)&p_cmd_table->p_cfis;
    // __builtin_memset(p_cfis, 0, sizeof(*p_cfis));
    p_cfis->fis_type = SATA_FIS_REG_H2D;
    p_cfis->b_cmd = true;
    p_cfis->command = cmd.command;
    p_cfis->device = cmd.device;
    p_cfis->features_7_0 = cmd.features & 0xFF;
    p_cfis->features_15_8 = (cmd.features >> 8) & 0xFF;
    p_cfis->lba0 = (cmd.lba >> 0) & 0xFF;
    p_cfis->lba1 = (cmd.lba >> 8) & 0xFF;
    p_cfis->lba2 = (cmd.lba >> 16) & 0xFF;
    p_cfis->lba3 = (cmd.lba >> 24) & 0xFF;
    p_cfis->lba4 = (cmd.lba >> 32) & 0xFF;
    p_cfis->lba5 = (cmd.lba >> 40) & 0xFF;
    p_cfis->count = cmd.count;

    // Wait until the port is no longer busy.
    size_t spin = 0;
    while (spin < 100000) {
        ahci_port_tfd_t port_tfd;
        kmemread_v4(&port_tfd, &port_ctx->reg_port->tfd);
        if ((port_tfd.sts & (AHCI_TFD_STS_BSY | AHCI_TFD_STS_DRQ)) == 0) {
            break;
        }
    }
    if (spin >= 100000) {
        kprintf("ahci: %s: port is busy\n", port_ctx->name);
        return false;
    }

    // Clear the D2H Register FIS interrupt flag.
    port_ctx->reg_port->is = AHCI_PORT_INT_DHR;

    // Issue the command.
    port_ctx->reg_port->ci = (1 << cmd_slot);

    *p_cmd_slot = cmd_slot;
    return true;
}

/**
 * Waits for the command in slot @a cmd_slot to finish.
 *
 * @param port_ctx Port context.
 * @param cmd_slot Command slot to wait on.
 *
 * @returns
 * - `true` if the command has finished successfully.
 * - `false` if the Task File Error bit is set in the Port Interrupt Status
 *   register (IS.TFES), or if no FIS has been received at all.
 */
static bool wait_for_cmd(ahci_port_ctx_t *port_ctx, size_t cmd_slot) {
    reg_port_t *const reg_port = port_ctx->reg_port;

    bool b_has_err = false;
    while (true) {
        if (reg_port->is & AHCI_PORT_INT_TFE) {
            b_has_err = true;
            reg_port->is = AHCI_PORT_INT_TFE;
            kprintf("ahci: task file error\n");
            break;
        }

        if (!(reg_port->ci & (1 << cmd_slot))) { break; }
    }

    // We expect a D2H RFIS to arrive.
    if (!(reg_port->is & AHCI_PORT_INT_DHR)) {
        // RFIS did not arrive.
        kprintf("ahci: command completed, but RFIS was not received\n");
        return false;
    }

    // Clear RFIS interrupt flag.
    reg_port->is = AHCI_PORT_INT_DHR;

    // Check the error.
    if (b_has_err) {
        kprintf("ahci: %s: received FIS error is set to 0x02%x", port_ctx->name,
                port_ctx->p_rfis->rfis.error);
        if (port_ctx->p_rfis->rfis.error & SATA_ERROR_ABORT) {
            kprintf("; device aborted command");
        }
        kprintf("\n");
        return false;
    }

    return true;
}

static bool find_cmd_slot(reg_port_t *reg_port, size_t *out_slot) {
    ASSERT(out_slot);

    for (int cmd_slot = 0; cmd_slot < 32; cmd_slot++) {
        // Device Status bit.
        const bool b_ds = 0 != (reg_port->sact & (1 << cmd_slot));
        // Command Issued bit.
        const bool b_ci = 0 != (reg_port->ci & (1 << cmd_slot));

        if (!b_ds && !b_ci) {
            *out_slot = cmd_slot;
            return true;
        }
    }
    return false;
}
