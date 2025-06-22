/**
 * @file ahci.c
 * SATA AHCI driver implementation.
 *
 * Refer to Serial ATA Advanced Host Controller Interface (AHCI) 1.3.1.
 */

#include <stddef.h>

#include "assert.h"
#include "disk/ahci.h"
#include "disk/ahci_regs.h"
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
    char name[AHCI_PORT_NAME_SIZE];
    char serial_str[SATA_SERIAL_STR_LEN + 1];
    /// Port index in the controller.
    size_t port_num;
    /// Port is online and is a SATA device.
    bool online_sata;
    bool identified;

    ahci_ctrl_ctx_t *ctrl_ctx;
    reg_port_t *reg_port;
    uint32_t num_sectors;

    /// Received FIS buffer.
    ahci_rfis_t *p_rfis;
    ahci_cmd_hdr_t *p_cmd_list;
    ahci_cmd_table_t *p_cmd_tables;
};

/**
 * AHCI Controller context.
 */
struct ahci_ctrl_ctx {
    /// Unique device name.
    char name[AHCI_CTRL_NAME_SIZE];

    /// PCI device context.
    const pci_dev_t *pci_dev;

    /// Generic HBA Control register.
    reg_ghc_t *reg_ghc;

    /**
     * Implemented port contexts.
     *
     * The controller manifests its number of ports in its HBA Capability
     * register, field NP (Number of Ports). However, the number of ports it
     * implements may be less than that and is specified in the PI (Ports
     * Implemented) register.
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
static bool prv_ahci_identify_port(ahci_port_ctx_t *port_ctx);

static bool prv_ahci_read_port(ahci_port_ctx_t *port_ctx, uint64_t start_sector,
                               uint32_t num_sectors, void *p_buf);

static bool send_read_cmd(ahci_port_ctx_t *port_ctx, ata_cmd_t cmd, void *p_buf,
                          size_t read_len, size_t *p_cmd_slot);
static bool wait_for_cmd(ahci_port_ctx_t *port_ctx, size_t cmd_slot);
static bool find_cmd_slot(reg_port_t *reg_port, size_t *out_slot);

ahci_ctrl_ctx_t *ahci_ctrl_new(const pci_dev_t *pci_dev) {
    const uint32_t abar = pci_dev->header.bar5;
    const uint32_t hba_regs_addr = abar & AHCI_ABAR_ADDR_MASK;

    for (uint32_t page = hba_regs_addr;
         page < hba_regs_addr + AHCI_HBA_REGS_SIZE; page += 4096) {
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
            port_ctx->identified = prv_ahci_identify_port(port_ctx);
        }
    }

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

bool ahci_port_is_online(const ahci_port_ctx_t *port_ctx) {
    return port_ctx->online_sata;
}

const char *ahci_port_name(const ahci_port_ctx_t *port_ctx) {
    return port_ctx->name;
}

bool ahci_port_read(ahci_port_ctx_t *port_ctx, uint64_t start_sector,
                    uint32_t num_sectors, void *p_buf) {
    return prv_ahci_read_port(port_ctx, start_sector, num_sectors, p_buf);
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

    if (p_ghc->ghc_bit.ae) {
        // AE set means AHCI mode is enabled.
        return true;
    }

    if (p_ghc->cap_bit.sam) {
        // SAM set means the SATA controller supports both AHCI and legacy
        // interfaces. Enable AHCI.
        p_ghc->ghc_bit.ae = 1;

        // Ensure that AE was set.
        if (p_ghc->ghc_bit.ae) {
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

static void prv_ahci_enumerate_ports(ahci_ctrl_ctx_t *ctrl_ctx) {
    reg_ghc_t *const reg_ghc = ctrl_ctx->reg_ghc;

    kprintf("ahci: %s: port cabaility: %u\n", ctrl_ctx->name,
            reg_ghc->cap_bit.np + 1);
    kprintf("ahci: %s: implemented ports: 0x%08X\n", ctrl_ctx->name,
            reg_ghc->pi);

    for (size_t port_idx = 0; port_idx < AHCI_PORTS_PER_CTRL; port_idx++) {
        ahci_port_ctx_t *const port_ctx = &ctrl_ctx->ports[port_idx];
        kmemset(port_ctx, 0, sizeof(*port_ctx));

        port_ctx->port_num = port_idx;
        port_ctx->online_sata = false;
        port_ctx->ctrl_ctx = ctrl_ctx;
        port_ctx->reg_port = AHCI_REG_PORT(reg_ghc, port_idx);
        prv_ahci_set_port_name(port_ctx);

        const bool impl = (reg_ghc->pi & (1 << port_idx)) != 0;
        if (!impl) { continue; }

        reg_port_t *const reg_port = port_ctx->reg_port;
        switch (reg_port->ssts_bit.det) {
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
    reg_port->cmd_bit.st = 0;
    reg_port->cmd_bit.fre = 0;
    while (reg_port->cmd_bit.fr || reg_port->cmd_bit.cr) {}

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
    reg_port->cmd_bit.fre = 1;
    reg_port->cmd_bit.st = 1;

    // Wait for them to start.
    while (!reg_port->cmd_bit.cr || !reg_port->cmd_bit.fr) {}
}

/// Sends an
static bool prv_ahci_identify_port(ahci_port_ctx_t *port_ctx) {
    ASSERT(port_ctx);
    ASSERT(port_ctx->reg_port);

    ata_cmd_t cmd = {0};
    cmd.command = SATA_CMD_IDENTIFY_DEVICE;

    uint16_t *const p_ident = heap_alloc_aligned(512, 2);
    size_t cmd_slot;
    if (!send_read_cmd(port_ctx, cmd, p_ident, 512, &cmd_slot)) {
        kprintf("ahci: %s: could not issue IDENTIFY_DEVICE\n", port_ctx->name);
        heap_free(p_ident);
        return false;
    }

    bool b_ok = wait_for_cmd(port_ctx, cmd_slot);
    if (!b_ok) {
        kprintf("ahci: %s: command IDENTIFY_DEVICE failed\n", port_ctx->name);
        heap_free(p_ident);
        return false;
    }

    kmemcpy(port_ctx->serial_str, p_ident + 10, SATA_SERIAL_STR_LEN);
    port_ctx->num_sectors = *((uint32_t *)&p_ident[60]);

    kprintf("ahci: %s: serial is '%s'\n", port_ctx->name, port_ctx->serial_str);
    kprintf("ahci: %s: number of sectors: %u\n", port_ctx->name,
            port_ctx->num_sectors);

    heap_free(p_ident);
    return true;
}

static bool prv_ahci_read_port(ahci_port_ctx_t *port_ctx, uint64_t start_sector,
                               uint32_t num_sectors, void *p_buf) {
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
    size_t cmd_slot;
    if (!send_read_cmd(port_ctx, cmd, p_buf, 512 * num_sectors, &cmd_slot)) {
        kprintf("ahci: %s: failed to issue read command\n", port_ctx->name);
        return false;
    }

    // Wait for its completion.
    bool b_ok = wait_for_cmd(port_ctx, cmd_slot);
    if (!b_ok) {
        kprintf("ahci: command failed\n");
        return false;
    }

    return true;
}

/**
 * Sends an ATA command to read from a device.
 *
 * @param[in]  port_ctx   Context of the port that will handle the command.
 * @param[in]  cmd        ATA command details.
 * @param[in]  p_buf      Output buffer to copy the data read from the device.
 * @param[in]  read_len   Number of bytes to be received into @a p_buf.
 * @param[out] p_cmd_slot Command slot pointer.
 *
 * If the command has been issued, writes its slot to @a p_cmd_slot.
 *
 * @returns `true` if the command has been issued, otherwise `false`.
 *
 * @warning
 * Does not check if the command was aborted, or for any other ATA error.
 */
static bool send_read_cmd(ahci_port_ctx_t *port_ctx, ata_cmd_t cmd, void *p_buf,
                          size_t read_len, size_t *p_cmd_slot) {
    // One PRD can describe a 4 MiB block of data.
    const uint16_t prdtl =
        ((read_len + ((4 * 1024 * 1024) - 1)) / (4 * 1024 * 1024));
    if (prdtl > AHCI_CMD_TABLE_NUM_PRDS) {
        kprintf("ahci: %s: cannot transfer %u bytes of data\n", port_ctx->name,
                read_len);
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
    p_cmd_hdr->prdtl = prdtl;

    // Fill the command table.  Start with the PRD table.  We have N PRDs, each
    // can describe 4 MiBs.
    ahci_cmd_table_t *const p_cmd_table = &port_ctx->p_cmd_tables[cmd_slot];
    for (size_t prd_idx = 0; prd_idx < prdtl; prd_idx++) {
        ahci_prd_t *const p_prd = &p_cmd_table->p_prd_table[prd_idx];
        p_prd->dba = (((uint32_t)p_buf) + (4 * 1024 * 1024 * prd_idx));
        p_prd->dbau = 0;
        p_prd->dbc = ((read_len - (4 * 1024 * 1024 * prd_idx)) - 1);
        p_prd->b_int = true;
    }

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
    while ((port_ctx->reg_port->tfd_bit.sts &
            (AHCI_TFD_STS_BSY | AHCI_TFD_STS_DRQ)) &&
           (spin++ < 100000)) {}
    if (spin >= 100000) {
        kprintf("ahci: %s: port is busy\n", port_ctx->name);
        return false;
    }

    // Clear the D2H Register FIS interrupt flag.
    port_ctx->reg_port->is_bit.dhrs = 0;

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
 *
 * @note
 * Use this function right after issuing a command, e.g., using
 * #send_read_cmd(), so that the D2H RFIS Interrupt (IS.DHRS) is not reset
 * between the calls.
 */
static bool wait_for_cmd(ahci_port_ctx_t *port_ctx, size_t cmd_slot) {
    reg_port_t *const reg_port = port_ctx->reg_port;

    bool b_has_err = false;
    while (true) {
        if (reg_port->is_bit.tfes) {
            b_has_err = true;
            reg_port->is_bit.tfes = 1; // write 1 to clear
            kprintf("ahci: task file error\n");
            break;
        }

        if (!(reg_port->ci & (1 << cmd_slot))) { break; }
    }

    // We expect a D2H RFIS to arrive.
    if (!reg_port->is_bit.dhrs) {
        // RFIS did not arrive.
        kprintf("ahci: command completed, but RFIS was not received\n");
        return false;
    }

    // Clear RFIS interrupt flag.
    reg_port->is_bit.dhrs = 1;

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
