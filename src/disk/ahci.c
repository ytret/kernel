/**
 * @file ahci.c
 * SATA AHCI driver implementation.
 *
 * Refer to Serial ATA Advanced Host Controller Interface (AHCI) 1.3.1.
 */

#include <stddef.h>

#include "disk/ahci.h"
#include "disk/ahci_regs.h"
#include "disk/sata.h"
#include "heap.h"
#include "kprintf.h"
#include "pci.h"
#include "vmm.h"

/**
 * Number of PRDT entries in each command table.
 * See #ahci_cmd_table_t.
 */
#define CMD_TABLE_NUM_PRDS 8

/**
 * ABAR register, base address mask.
 *
 * Although AHCI 1.3.1, section 2.1.11 (Offset 24h: ABAR - AHCI Base Address)
 * specifies bits 12..04 as reserved, QEMU uses the 12th bit. Maybe the whole
 * reserved field is meant to be used as an address, but that may take away its
 * page alignment property, which the code relies on.
 */
#define ABAR_ADDR_MASK (~0xFFF)

/**
 * Received Frame Information Structure (FIS).
 *
 * Frames that are received from the host are copied to the appropriate field of
 * this structure by the HBA.
 *
 * Refer to section 4.2.1.
 */
typedef volatile struct __attribute__((packed)) {
    /// DMA Setup FIS.
    sata_fis_dma_setup_t dsfis __attribute__((aligned(0x20)));
    /// Port IO Setup FIS.
    sata_fis_pio_setup_t psfis __attribute__((aligned(0x20)));
    /// Device-to-HBA FIS.
    sata_fis_reg_d2h_t rfis __attribute__((aligned(0x20)));
    /// Set-Device-Bits FIS.
    sata_fis_dev_bits_t sdbfis __attribute__((aligned(0x08)));

    /// Unknown FIS.
    uint8_t p_ufis[64];
    uint8_t _reserved[96];
} ahci_rfis_t;

/**
 * Command Header.
 * Refer to section 4.2.2.
 */
typedef volatile struct __attribute__((packed)) {
    // DW 0 - Description Information.
    /// Command FIS Length.
    uint8_t cfl : 5;
    /// ATAPI bit: PIO Setup FIS shall be sent (1) or not (0).
    bool b_atapi : 1;
    /// Data direction bit: write to the device (1) or read from it (0).
    bool b_write : 1;
    /// Prefetchable bit.
    bool b_prefetch : 1;
    /// Reset bit.
    bool b_reset : 1;
    /// BIST bit: the command is for sending a BIST FIS (1) or not (0).
    bool b_bist : 1;
    /// Clear BSY upon R_OK (1) or not (0).
    bool b_clear_bsy : 1;
    uint8_t _reserved_1 : 1;
    /// Port Multiplier Port.
    uint8_t pmp : 4;
    /// Physical Region Descriptor Table Length.
    uint16_t prdtl;

    // DW 1 - Command Status.
    uint32_t prdbc; //!< Physical Region Descriptor Byte Count.

    // DW 2.
    uint32_t ctba; //!< Command Table Descriptor Base Address.

    // DW 3.
    uint32_t ctbau; //!< Upper 32 Bits of CTBA.

    uint32_t _reserved_2[4];
} ahci_cmd_hdr_t;

/**
 * Physical Region Descriptor Table (PRDT).
 * Refer to section 4.2.3.3.
 */
typedef volatile struct __attribute__((packed)) {
    // DW 0.
    uint32_t dba; //!< Data Base Address.
    // DW 1.
    uint32_t dbau; //!< Upper 32 Bits of DBA.
    // DW 2.
    uint32_t _reserved_1;

    // DW 3.
    uint32_t dbc : 22; //!< Data Byte Count.
    uint16_t _reserved_2 : 9;
    bool b_int : 1; //!< Interrupt On Completion.
} ahci_prd_t;

/**
 * Command Table.
 * Refer to section 4.2.3.
 */
typedef volatile struct __attribute__((packed)) {
    uint8_t p_cfis[64]; //!< Command FIS.
    uint8_t p_acmd[16]; //!< ATAPI Command.
    uint8_t _reserved[48];

    /**
     * Physical Region Descriptor Table (PRDT).
     * The number of items in this table is set by the #ahci_cmd_hdr_t.prdtl
     * field in the Command Header.
     */
    ahci_prd_t p_prd_table[CMD_TABLE_NUM_PRDS];
} ahci_cmd_table_t;

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

_Static_assert(0x2C == sizeof(reg_ghc_t), "");
_Static_assert(0x80 == sizeof(reg_port_t), "");
_Static_assert(0x100 == sizeof(ahci_rfis_t), "");
_Static_assert(0x20 == sizeof(ahci_cmd_hdr_t), "");
_Static_assert(0x10 == sizeof(ahci_prd_t), "");

/// Driver state.
static bool gb_initialized;

/**
 * @{
 * @name Root disk
 */
static void *gp_hba;
static reg_port_t *gp_port;
static uint32_t g_max_sectors;
/// @}

/**
 * @{
 * @name FIS buffers
 */
static ahci_rfis_t g_rfis __attribute__((aligned(256)));
static ahci_cmd_hdr_t gp_cmd_list[32] __attribute__((aligned(1024)));
static ahci_cmd_table_t gp_cmd_tables[1] __attribute__((aligned(128)));
/// @}

static bool ensure_ahci_mode(void);
static bool find_root_port(void);
static bool identify_device(void);

static bool read_sectors(reg_port_t *p_port, uint64_t start_sector,
                         uint32_t num_sectors, void *p_buf);

static int send_read_cmd(reg_port_t *p_port, ata_cmd_t cmd, void *p_buf,
                         size_t read_len);
static bool wait_for_cmd(reg_port_t *p_port, int cmd_slot);
static int find_cmd_slot(reg_port_t *p_port);

static void dump_port_reg(reg_port_t *p_port) __attribute__((unused));

bool ahci_init(uint8_t bus, uint8_t dev) {
    // Get the HBA memory registers address.
    uint8_t p_config_u8[PCI_CONFIG_SIZE];
    pci_read_config(bus, dev, p_config_u8);

    pci_config_t *p_config = ((pci_config_t *)p_config_u8);
    uint32_t abar = p_config->bar5;
    uint32_t hba_mem_addr = (abar & ABAR_ADDR_MASK);

    // Map the HBA memory registers.
    size_t page_count = 0;
    for (uint32_t page = hba_mem_addr;
         page < (hba_mem_addr + AHCI_HBA_REGS_SIZE); page += 4096) {
        vmm_map_kernel_page(page, page);
        page_count++;
    }

    // Set the root disk HBA.
    gp_hba = ((void *)hba_mem_addr);

    bool b_ahci = ensure_ahci_mode();
    if (!b_ahci) { return false; }

    bool b_port = find_root_port();
    if (!b_port) { return false; }

    // Stop FIS receive and command list DMA engines.
    gp_port->cmd_bit.st = 0;
    gp_port->cmd_bit.fre = 0;
    while (gp_port->cmd_bit.fr || gp_port->cmd_bit.cr) {}

    // Initialzie the buffer pointers.
    gp_port->clb = ((uint32_t)gp_cmd_list);
    gp_port->clbu = 0;
    gp_port->fb = ((uint32_t)&g_rfis);
    gp_port->fbu = 0;

    ahci_cmd_hdr_t *p_cmd_hdr = &gp_cmd_list[0];
    p_cmd_hdr->ctba = ((uint32_t)&gp_cmd_tables[0]);
    p_cmd_hdr->ctbau = 0;

    // Start the command list DMA engine.
    gp_port->cmd_bit.fre = 1;
    gp_port->cmd_bit.st = 1;

    // Wait for them to start.
    while (!gp_port->cmd_bit.cr || !gp_port->cmd_bit.fr) {}

    bool b_id = identify_device();
    if (!b_id) { return false; }

    gb_initialized = true;
    return true;
}

bool ahci_read_sectors(uint64_t start_sector, uint32_t num_sectors,
                       void *p_buf) {
    return read_sectors(gp_port, start_sector, num_sectors, p_buf);
}

bool ahci_is_init(void) {
    return gb_initialized;
}

/**
 * Enables AHCI mode on the @ref gp_hba "root HBA".
 * @returns `true` if the controller is in or has been placed in AHCI mode,
 * otherwise returns `false`.
 */
static bool ensure_ahci_mode(void) {
    reg_ghc_t *p_ghc = gp_hba;

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

static bool find_root_port(void) {
    reg_ghc_t *p_ghc = gp_hba;

    kprintf("ahci: ports implemented: 0x%08X\n", p_ghc->pi);
    for (size_t port = 0; port < 32; port++) {
        if (!(p_ghc->pi & (1 << port))) {
            // Skip an unimplemented port.
            continue;
        }

        // Check the device detection bits.
        reg_port_t *p_port = ((reg_port_t *)HBA_REG_PORT(gp_hba, port));
        switch (p_port->ssts_bit.det) {
        case AHCI_SSTS_DET_DEV_NPHY:
            kprintf("ahci: port %u: device detected, but there is no"
                    " communication\n",
                    port);
            break;

        case AHCI_SSTS_DET_DEV_PHY:
            kprintf("ahci: port %u: signature = %08x ", port, p_port->sig);
            switch (p_port->sig) {
            case SATA_SIG_ATA:
                kprintf("(ATA)\n");
                gp_port = p_port;
                return true;

            default:
                kprintf("(unknown)\n");
            }
            break;

        case AHCI_SSTS_DET_PHY_OFF:
            kprintf("ahci: port %u: phy in offline mode\n", port);
            break;
        }
    }

    return false;
}

static bool identify_device(void) {
    ata_cmd_t cmd = {0};
    cmd.command = SATA_CMD_IDENTIFY_DEVICE;

    uint16_t *p_ident = heap_alloc_aligned(512, 2);
    int cmd_slot = send_read_cmd(gp_port, cmd, p_ident, 512);
    if (-1 == cmd_slot) {
        kprintf("ahci: could not issue identify command\n");
        return false;
    }

    bool b_ok = wait_for_cmd(gp_port, cmd_slot);
    if (!b_ok) {
        kprintf("ahci: identify command failed\n");
        return false;
    }

    char p_serial[21] = {0};
    __builtin_memcpy(p_serial, (p_ident + 10), 20);
    kprintf("ahci: serial number is '%s'\n", p_serial);

    g_max_sectors = *((uint32_t *)&p_ident[60]);

    uint32_t disk_size_kib = (g_max_sectors / 2);
    uint32_t disk_size_mib = (disk_size_kib / 1024);
    uint32_t disk_size_gib = (disk_size_mib / 1024);

    kprintf("ahci: disk size is");
    if (disk_size_mib > 9999) {
        kprintf(" %u GiB\n", disk_size_gib);
    } else if (disk_size_kib > 9999) {
        kprintf(" %u MiB\n", disk_size_mib);
    } else {
        kprintf(" %u KiB\n", disk_size_kib);
    }

    heap_free(p_ident);

    return true;
}

static bool read_sectors(reg_port_t *p_port, uint64_t start_sector,
                         uint32_t num_sectors, void *p_buf) {
    // Check start_sector.
    if (start_sector >> 48) {
        kprintf("ahci: start sector number cannot be wider than 48 bits\n");
        return false;
    }
    if (start_sector >= g_max_sectors) {
        kprintf("ahci: start sector is past disk end by %u sectors\n",
                (start_sector - g_max_sectors));
        return false;
    }

    // Check num_sectors.
    if (0 == num_sectors) { return true; }
    if ((start_sector + num_sectors) > g_max_sectors) {
        kprintf("ahci: cannot read past disk end by %u sectors\n",
                (start_sector + num_sectors) - g_max_sectors);
        return false;
    }

    // We have a finite amount of PRDs.  Each can describe a 4 MiB data block,
    // or 8192 512-byte sectors.
    if (num_sectors > (8192 * CMD_TABLE_NUM_PRDS)) {
        kprintf("ahci: number of sectors to read cannot be greater than %u\n",
                (8192 * CMD_TABLE_NUM_PRDS));
        return false;
    }

    // Set up a command.
    ata_cmd_t cmd = {0};
    cmd.count = num_sectors;
    cmd.lba = start_sector;
    cmd.device = (1 << 6);
    cmd.command = SATA_CMD_READ_DMA_EXT;

    // Issue the command.
    int cmd_slot = send_read_cmd(p_port, cmd, p_buf, (512 * num_sectors));
    if (-1 == cmd_slot) {
        kprintf("ahci: failed to issue read command\n");
        return false;
    }

    // Wait for its completion.
    bool b_ok = wait_for_cmd(p_port, cmd_slot);
    if (!b_ok) {
        kprintf("ahci: command failed\n");
        return false;
    }

    return true;
}

/**
 * Sends an ATA command to read from a device.
 *
 * @param p_port   Port Register of the port that will handle the command.
 * @param cmd      ATA command details.
 * @param p_buf    Output buffer to copy the data read from the device.
 * @param read_len Number of bytes to be received into @a p_buf.
 *
 * @returns Command slot of the issued command on success, otherwise -1.
 *
 * @warning
 * Does not check if the command was aborted, or for any other ATA error.
 */
static int send_read_cmd(reg_port_t *p_port, ata_cmd_t cmd, void *p_buf,
                         size_t read_len) {
    // One PRD can describe a 4 MiB block of data.
    uint16_t prdtl = ((read_len + ((4 * 1024 * 1024) - 1)) / (4 * 1024 * 1024));
    if (prdtl > CMD_TABLE_NUM_PRDS) {
        kprintf("ahci: cannot transfer %u bytes of data\n", read_len);
        return -1;
    }

    // Find a free command slot.
    int cmd_slot = find_cmd_slot(p_port);
    if (-1 == cmd_slot) {
        kprintf("ahci: could not find free command slot\n");
        return -1;
    }

    // Set up the command header.
    ahci_cmd_hdr_t *p_cmd_hdr = &gp_cmd_list[cmd_slot];
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
    for (size_t prd_idx = 0; prd_idx < prdtl; prd_idx++) {
        ahci_prd_t *p_prd = &gp_cmd_tables[0].p_prd_table[prd_idx];
        p_prd->dba = (((uint32_t)p_buf) + (4 * 1024 * 1024 * prd_idx));
        p_prd->dbau = 0;
        p_prd->dbc = ((read_len - (4 * 1024 * 1024 * prd_idx)) - 1);
        p_prd->b_int = true;
    }

    // Finish by setting the CFIS.
    sata_fis_reg_h2d_t *p_cfis =
        ((sata_fis_reg_h2d_t *)&gp_cmd_tables[0].p_cfis);
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
    while ((p_port->tfd_bit.sts & (AHCI_TFD_STS_BSY | AHCI_TFD_STS_DRQ)) &&
           (spin++ < 100000)) {}
    if (spin >= 100000) {
        kprintf("ahci: port is busy\n");
        return -1;
    }

    // Clear the D2H Register FIS interrupt flag.
    p_port->is_bit.dhrs = 0;

    // Issue the command.
    p_port->ci = (1 << cmd_slot);

    return cmd_slot;
}

/**
 * Waits for the command in slot @a cmd_slot to finish.
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
static bool wait_for_cmd(reg_port_t *p_port, int cmd_slot) {
    bool b_has_err = false;
    while (true) {
        if (p_port->is_bit.tfes) {
            b_has_err = true;
            p_port->is_bit.tfes = 1; // write 1 to clear
            kprintf("ahci: task file error\n");
            break;
        }

        if (!(p_port->ci & (1 << cmd_slot))) { break; }
    }

    // We expect a D2H RFIS to arrive.
    if (!p_port->is_bit.dhrs) {
        // RFIS did not arrive.
        kprintf("ahci: command completed, but RFIS was not received\n");
        return false;
    }

    // Clear RFIS interrupt flag.
    p_port->is_bit.dhrs = 1;

    // Check the error.
    if (b_has_err) {
        kprintf("ahci: Error register is set to %x", g_rfis.rfis.error);
        if (g_rfis.rfis.error & SATA_ERROR_ABORT) {
            kprintf("; device aborted command");
        }
        kprintf("\n");
        return false;
    }

    return true;
}

static int find_cmd_slot(reg_port_t *p_port) {
    for (int slot = 0; slot < 32; slot++) {
        bool b_ds = (0 != (p_port->sact & (1 << slot))); // device status
        bool b_ci = (0 != (p_port->ci & (1 << slot)));   // command issued
        if ((!b_ds) && (!b_ci)) { return slot; }
    }
    return -1;
}

static void dump_port_reg(reg_port_t *p_port) {
    kprintf("Port register at %P:\n", p_port);
    kprintf("    clb = %08x\n", p_port->clb);
    kprintf("   clbu = %08x\n", p_port->clbu);
    kprintf("     fb = %08x\n", p_port->fb);
    kprintf("    fbu = %08x\n", p_port->fbu);
    kprintf("     is = %08x\n", p_port->is);
    kprintf("     ie = %08x\n", p_port->ie);
    kprintf("    cmd = %08x\n", p_port->cmd);
    kprintf("    tfd = %08x\n", p_port->tfd);
    kprintf("    sig = %08x\n", p_port->sig);
    kprintf("   ssts = %08x\n", p_port->ssts);
    kprintf("   sctl = %08x\n", p_port->sctl);
    kprintf("   serr = %08x\n", p_port->serr);
    kprintf("   sact = %08x\n", p_port->sact);
    kprintf("     ci = %08x\n", p_port->ci);
    kprintf("   sntf = %08x\n", p_port->sntf);
    kprintf("    fbs = %08x\n", p_port->fbs);
    kprintf(" devslp = %08x\n", p_port->devslp);
}
