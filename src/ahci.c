#include <ahci.h>
#include <alloc.h>
#include <pci.h>
#include <printf.h>
#include <sata.h>
#include <vmm.h>

#include <stddef.h>

// Number of PRDT entries in each command table.
//
#define CMD_TABLE_NUM_PRDS      8

// Although AHCI 1.3.1 specifies 12..04 as reserved (section 2.1.11), QEMU uses
// the 12th bit.  Maybe the whole reserved field is meant to be used as an
// address, but that may take away its page alignment, which the code relies on.
//
#define ABAR_ADDR_MASK          (~0xFFF)

// HBA memory reigsters defines.
//
#define HBA_MEM_SIZE            0x1100
#define HBA_REG_GHC_OFFSET      0
#define HBA_REG_PORT_OFFSET(P)  (0x100 + (0x80 * (P)))
#define HBA_REG_PORT(HBA, P)                                    \
    (((uint8_t const *) (HBA)) + (HBA_REG_PORT_OFFSET(P)))

// Bits of GHC register fields.
//
#define GHC_GHC_AE              (1 << 31)  // AHCI enable
#define GHC_CAP_SAM             (1 << 18)  // supports AHCI mode only

// Bits of port register.
#define PORT_IS_TFES            (1 << 30)    // task file error status
#define PORT_IS_PSS             (1 << 1)     // PIO setup FIS interrupt
#define PORT_IS_DHRS            (1 << 0)     // device to host register FIS int.
#define PORT_CMD_CR             (1 << 15)    // command list running
#define PORT_CMD_FR             (1 << 14)    // FIS receive running
#define PORT_CMD_FRE            (1 << 4)     // FIS receive enable
#define PORT_CMD_ST             (1 << 0)     // start command list processing
#define PORT_TFD_BSY            (1 << 7)     // interface is busy
#define PORT_TFD_DRQ            (1 << 3)     // data tranfser is requested
#define PORT_SSTS_IPM_MASK      (0x1F << 8)  // interface power management
#define PORT_SSTS_DET_MASK      (0x0F << 0)  // device detection

typedef volatile struct
__attribute__ ((packed))
{
    uint32_t cap;        // host capabilities
    uint32_t ghc;        // global host control
    uint32_t is;         // interrupt status
    uint32_t pi;         // ports implemented
    uint32_t vs;         // version
    uint32_t ccc_ctl;    // command completion coalescing control
    uint32_t ccc_ports;  // command completion coalescing ports
    uint32_t em_loc;     // enclosure management location
    uint32_t em_ctl;     // enclosure management control
    uint32_t cap2;       // host capabilities extended
    uint32_t bohc;       // BIOS/OS handoff control and status
} reg_ghc_t;

typedef volatile struct
__attribute__ ((packed))
{
    uint32_t clb;             // command list base address
    uint32_t clbu;            // upper 32 bits of clb
    uint32_t fb;              // FIS base address
    uint32_t fbu;             // upper 32 bits of fb
    uint32_t is;              // interrupt status
    uint32_t ie;              // interrupt enable
    uint32_t cmd;             // command and status
    uint32_t _reserved_1;     // reserved
    uint32_t tfd;             // task file data
    uint32_t sig;             // signature
    uint32_t ssts;            // SATA status
    uint32_t sctl;            // SATA control
    uint32_t serr;            // SATA error
    uint32_t sact;            // SATA active
    uint32_t ci;              // command issue
    uint32_t sntf;            // SATA notification
    uint32_t fbs;             // FIS-based switching control
    uint32_t devslp;          // device sleep
    uint32_t _reserved_2[10]; // reserved
    uint32_t p_vs[4];         // vendor specific
} reg_port_t;

typedef volatile struct
__attribute__ ((packed))
{
    sata_fis_dma_setup_t dsfis  __attribute__ ((aligned(0x20)));
    sata_fis_pio_setup_t psfis  __attribute__ ((aligned(0x20)));
    sata_fis_reg_d2h_t   rfis   __attribute__ ((aligned(0x20)));
    sata_fis_dev_bits_t  sdbfis __attribute__ ((aligned(0x08)));

    uint8_t p_ufis[64];
    uint8_t _reserved[96];
} rfis_t;

typedef volatile struct
__attribute__ ((packed))
{
    // 0x00
    uint8_t  cfl:5;  // command FIS length
    bool     b_atapi:1;
    bool     b_write:1;
    bool     b_prefetch:1;
    bool     b_reset:1;
    bool     b_bist:1;
    bool     b_clear_bsy:1;
    uint8_t  _reserved_1:1;
    uint8_t  pmp:4;  // port multiplier port
    uint16_t prdtl;  // physical region descriptor table length

    uint32_t prdbc;  // 0x08 - physical region descriptor byte count
    uint32_t ctba;   // 0x0C - command table descriptor base address
    uint32_t ctbau;  // 0x10

    uint32_t _reserved_2[4];
} cmd_hdr_t;

typedef volatile struct
__attribute__ ((packed))
{
    uint32_t dba;          // 0x00 - data base address
    uint32_t dbau;         // 0x04
    uint32_t _reserved_1;  // 0x08

    // 0x0C
    uint32_t dbc:22;
    uint16_t _reserved_2:9;
    bool     b_int:1;
} prd_t;

typedef volatile struct
__attribute__ ((packed))
{
    uint8_t p_cfis[64];
    uint8_t p_acmd[16];
    uint8_t _reserved[48];

    prd_t   p_prd_table[CMD_TABLE_NUM_PRDS];
} cmd_table_t;

typedef struct
{
    uint16_t feature;
    uint16_t count;
    uint64_t lba;
    uint8_t  device;
    uint8_t  command;
} cmd_t;

_Static_assert( 0x2C == sizeof(reg_ghc_t));
_Static_assert( 0x80 == sizeof(reg_port_t));
_Static_assert(0x100 == sizeof(rfis_t));
_Static_assert( 0x20 == sizeof(cmd_hdr_t));
_Static_assert( 0x10 == sizeof(prd_t));

// Root disk.
//
static void       * gp_hba;
static reg_port_t * gp_port;

// FIS buffers.
//
static volatile rfis_t      g_rfis           __attribute__ ((aligned(256)));
static volatile cmd_hdr_t   gp_cmd_list[32]  __attribute__ ((aligned(1024)));
static volatile cmd_table_t gp_cmd_tables[1] __attribute__ ((aligned(128)));

static bool ensure_ahci_mode(void);
static bool find_root_port(void);
static bool identify_device(void);

static bool read_sectors(reg_port_t * p_port, uint64_t offset,
                         uint32_t num_sectors, void * p_buf);

static int  send_read_cmd(reg_port_t * p_port, cmd_t cmd, void * p_buf,
                          size_t read_len);
static bool wait_for_cmd(reg_port_t * p_port, int cmd_slot);
static int  find_cmd_slot(reg_port_t * p_port);

static void dump_port_reg(reg_port_t * p_port) __attribute__ ((unused));

bool
ahci_init (uint8_t bus, uint8_t dev)
{
    // Get the HBA memory registers address.
    //
    uint8_t p_config_u8[PCI_CONFIG_SIZE];
    pci_read_config(bus, dev, p_config_u8);

    pci_config_t * p_config     = ((pci_config_t *) p_config_u8);
    uint32_t       abar         = p_config->bar5;
    uint32_t       hba_mem_addr = (abar & ABAR_ADDR_MASK);
    printf("ahci: abar = %P, base addr = %P\n", abar, hba_mem_addr);

    // Map the HBA memory registers.
    //
    size_t page_count = 0;
    for (uint32_t page = hba_mem_addr;
         page < (hba_mem_addr + HBA_MEM_SIZE);
         page += 4096)
    {
        vmm_map_kernel_page(page, page);
        page_count++;
    }
    printf("ahci: allocated %u pages for HBA memory registers\n", page_count);

    // Set the root disk HBA.
    //
    gp_hba = ((void *) hba_mem_addr);

    bool b_ahci = ensure_ahci_mode();
    if (!b_ahci)
    {
        return (false);
    }

    bool b_port = find_root_port();
    if (!b_port)
    {
        return (false);
    }

    // Stop FIS receive and command list DMA engines.
    //
    gp_port->cmd &= ~PORT_CMD_ST;
    gp_port->cmd &= ~PORT_CMD_FRE;
    while ((gp_port->cmd & PORT_CMD_FR) || (gp_port->cmd & PORT_CMD_CR))
    {}

    // Initialzie the buffer pointers.
    //
    gp_port->clb  = ((uint32_t) gp_cmd_list);
    gp_port->clbu = 0;
    gp_port->fb   = ((uint32_t) &g_rfis);
    gp_port->fbu  = 0;

    cmd_hdr_t * p_cmd_hdr  = &gp_cmd_list[0];
    p_cmd_hdr->ctba        = ((uint32_t) &gp_cmd_tables[0]);
    p_cmd_hdr->ctbau       = 0;

    // Start the command list DMA engine.
    //
    gp_port->cmd |= PORT_CMD_FRE;
    gp_port->cmd |= PORT_CMD_ST;

    // Wait for them to start.
    //
    while ((!(gp_port->cmd & PORT_CMD_CR)) || (!(gp_port->cmd & PORT_CMD_FR)))
    {}

    bool b_id = identify_device();
    if (!b_id)
    {
        return (false);
    }

    uint8_t * p_buf  = alloc_aligned(512 * 9, 2);
    bool      b_read = read_sectors(gp_port, 0, 9, p_buf);
    if (!b_read)
    {
        printf("ahci: failed to read bytes\n");
        return (false);
    }

    __builtin_memset(p_buf, 0, (512 * 9));
    b_read = read_sectors(gp_port, 9, 9, p_buf);
    if (!b_read)
    {
        printf("SECOND READ failed\n");
        return (false);
    }

    printf("p_buf = %P\n", p_buf);
    for (size_t sidx = 9; sidx < 9 + 9; sidx++)
    {
        for (size_t widx = 0; widx < 256; widx++)
        {
            size_t offset = ((512 * (sidx - 9)) + (2 * widx));

            uint16_t exp = ((2 * widx) + (sidx << 8));
            uint16_t act = ((p_buf[offset] << 8) | p_buf[offset + 1]);

            if (act != exp)
            {
                printf("sidx %u widx %u offset %04x is %04x, not %04x\n",
                       sidx, widx, offset, act, exp);
                return (false);
            }
        }
    }

    return (true);
}

/*
 * Returns true when the device is in AHCI mode, otherwise false.
 */
static bool
ensure_ahci_mode (void)
{
    reg_ghc_t * p_ghc = gp_hba;

    if (p_ghc->ghc & GHC_GHC_AE)
    {
        // AE set means AHCI mode is enabled.
        //
        return (true);
    }

    if (p_ghc->cap & GHC_CAP_SAM)
    {
        // SAM set means the SATA controller supports both AHCI and legacy
        // interfaces.  Enable AHCI.
        //
        p_ghc->ghc = GHC_GHC_AE;  // not ORed, see section 3.1.2, bit 31

        // Ensure that AE was set.
        //
        if (p_ghc->ghc & GHC_GHC_AE)
        {
            return (true);
        }
        else
        {
            printf("ahci: cannot set GHC.AE bit, when it must be R/W"
                   " because CAP.SAM is set\n");
            return (false);
        }
    }
    else
    {
        // SAM is zero, so the controller supports only AHCI mode.  However,
        // AE is zero, meaning that AHCI mode is disabled.
        //
        printf("ahci: CAP.SAM is zero, meaning that SATA controller"
               " supports only AHCI mode; but GHC.AE is zero, meaning that"
               " AHCI mode is disabled\n");
        return (false);
    }
}

static bool
find_root_port (void)
{
    reg_ghc_t * p_ghc = gp_hba;

    printf("ahci: ports implemented: 0x%08X\n", p_ghc->pi);
    for (size_t port = 0; port < 32; port++)
    {
        if (!(p_ghc->pi & (1 << port)))
        {
            // Skip an unimplemented port.
            //
            continue;
        }

        // Check the device detection bits.
        //
        reg_port_t * p_port = ((reg_port_t *) HBA_REG_PORT(gp_hba, port));
        switch (p_port->ssts & PORT_SSTS_DET_MASK)
        {
            case 1:
                printf("ahci: port %u: device detected, but there is no"
                       " communication\n", port);
            break;

            case 3:
                printf("ahci: port %u: signature = %08x ", port, p_port->sig);
                switch (p_port->sig)
                {
                    case SATA_SIG_ATA:
                        printf("(ATA)\n");
                        gp_port = p_port;
                        return (true);

                    default:
                        printf("(unknown)\n");
                }
            break;

            case 4:
                printf("ahci: port %u: phy in offline mode\n", port);
            break;
        }
    }

    return (false);
}

static bool
identify_device (void)
{
    cmd_t cmd   = { 0 };
    cmd.command = SATA_CMD_IDENTIFY_DEVICE;

    uint16_t * p_ident  = alloc_aligned(512, 2);
    int        cmd_slot = send_read_cmd(gp_port, cmd, p_ident, 512);
    if (-1 == cmd_slot)
    {
        printf("ahci: could not issue identify command\n");
        return (false);
    }

    bool b_ok = wait_for_cmd(gp_port, cmd_slot);
    if (!b_ok)
    {
        printf("ahci: identify command failed\n");
        return (false);
    }

    char p_serial[21] = { 0 };
    __builtin_memcpy(p_serial, (p_ident + 10), 20);
    printf("ahci: serial number is '%s'\n", p_serial);

    return (true);
}

static bool
read_sectors (reg_port_t * p_port, uint64_t start_sector, uint32_t num_sectors,
              void * p_buf)
{
    if (start_sector >> 48)
    {
        printf("ahci: start sector number cannot be wider than 48 bits\n");
        return (false);
    }

    if (0 == num_sectors)
    {
        return (true);
    }

    // We have a finite amount of PRDs.  Each can describe a 4 MiB data block,
    // or 8192 512-byte sectors.
    //
    if (num_sectors > (8192 * CMD_TABLE_NUM_PRDS))
    {
        printf("ahci: number of sectors to read cannot be greater than %u\n",
               (8192 * CMD_TABLE_NUM_PRDS));
        return (false);
    }

    cmd_t cmd   = { 0 };
    cmd.count   = num_sectors;
    cmd.lba     = start_sector;
    cmd.device  = (1 << 6);
    cmd.command = SATA_CMD_READ_DMA_EXT;
    int cmd_slot = send_read_cmd(p_port, cmd, p_buf, (512 * num_sectors));
    if (-1 == cmd_slot)
    {
        printf("ahci: failed to issue read command\n");
        return (false);
    }

    // Wait for completion.
    //
    bool b_ok = wait_for_cmd(p_port, cmd_slot);
    if (!b_ok)
    {
        printf("ahci: command failed\n");
        return (false);
    }

    return (true);
}

/*
 * Sends an ATA command to read from device using AHCI.
 *
 * Arguments:
 *   p_port   - port to issue the command to,
 *   cmd      - the ATA command itself,
 *   p_buf    - destination buffer of size read_len or bigger,
 *   read_len - number of bytes to be transferred.
 *
 * Returns command slot of the issued command on success, otherwise -1.
 *
 * NOTE: does not check if the command was aborted, or for any other ATA error.
 */
static int
send_read_cmd (reg_port_t * p_port, cmd_t cmd, void * p_buf, size_t read_len)
{
    // One PRD can describe a 4 MiB block of data.
    //
    uint16_t prdtl = ((read_len + ((4 * 1024 * 1024) - 1)) / (4 * 1024 * 1024));
    if (prdtl > CMD_TABLE_NUM_PRDS)
    {
        printf("ahci: cannot transfer %u bytes of data\n", read_len);
        return (-1);
    }

    // Find a free command slot.
    //
    int cmd_slot = find_cmd_slot(p_port);
    if (-1 == cmd_slot)
    {
        printf("ahci: could not find free command slot\n");
        return (-1);
    }

    // Set up the command header.
    //
    cmd_hdr_t * p_cmd_hdr  = &gp_cmd_list[cmd_slot];
    p_cmd_hdr->cfl         = (sizeof(sata_fis_reg_h2d_t) / 4);
    p_cmd_hdr->b_atapi     = false;
    p_cmd_hdr->b_write     = false;
    p_cmd_hdr->b_prefetch  = false;
    p_cmd_hdr->b_reset     = false;
    p_cmd_hdr->b_bist      = 0;
    p_cmd_hdr->b_clear_bsy = false;
    p_cmd_hdr->pmp         = 0;
    p_cmd_hdr->prdtl       = prdtl;

    // Fill the command table.  Start with the PRD table.  We have N PRDs, each
    // can describe 4 MiBs.
    //
    for (size_t prd_idx = 0; prd_idx < prdtl; prd_idx++)
    {
        prd_t * p_prd = &gp_cmd_tables[0].p_prd_table[prd_idx];
        p_prd->dba   = (((uint32_t) p_buf) + (4 * 1024 * 1024 * prd_idx));
        p_prd->dbau  = 0;
        p_prd->dbc   = ((read_len - (4 * 1024 * 1024 * prd_idx)) - 1);
        p_prd->b_int = true;
    }

    // Finish by setting the CFIS.
    //
    sata_fis_reg_h2d_t * p_cfis =
        ((sata_fis_reg_h2d_t *) &gp_cmd_tables[0].p_cfis);
    // __builtin_memset(p_cfis, 0, sizeof(*p_cfis));
    p_cfis->fis_type = SATA_FIS_REG_H2D;
    p_cfis->b_cmd    = true;
    p_cfis->command  = cmd.command;
    p_cfis->device   = cmd.device;
    p_cfis->lba0     = ((cmd.lba >> 0) & 0xFF);
    p_cfis->lba1     = ((cmd.lba >> 8) & 0xFF);
    p_cfis->lba2     = ((cmd.lba >> 16) & 0xFF);
    p_cfis->lba3     = ((cmd.lba >> 24) & 0xFF);
    p_cfis->lba4     = ((cmd.lba >> 32) & 0xFF);
    p_cfis->lba5     = ((cmd.lba >> 40) & 0xFF);
    p_cfis->count    = cmd.count;

    // Wait until the port is no longer busy.
    //
    size_t spin = 0;
    while ((p_port->tfd & (PORT_TFD_BSY | PORT_TFD_DRQ))
           && (spin++ < 100000))
    {}
    if (spin >= 100000)
    {
        printf("ahci: port is busy\n");
        return (-1);
    }

    // Clear RFIS interrupt flag.
    //
    p_port->is |= PORT_IS_DHRS;

    // Issue the command.
    //
    p_port->ci = (1 << cmd_slot);

    return (cmd_slot);
}

/*
 * Waits for the command in slot cmd_slot to finish.
 *
 * Returns false if Error bit is set in the received FIS, or if the FIS is not
 * received at all.  Otherwise returns true.
 *
 * NOTE: use this right after e.g. send_read_cmd(), so that IS.DHRS is not reset
 * between the calls.
 */
static bool
wait_for_cmd (reg_port_t * p_port, int cmd_slot)
{
    bool b_has_err = false;
    while (true)
    {
        if (p_port->is & PORT_IS_TFES)
        {
            b_has_err  = true;
            p_port->is = PORT_IS_TFES;
            printf("ahci: task file error\n");
            break;
        }

        if (!(p_port->ci & (1 << cmd_slot)))
        {
            break;
        }
    }

    // We expect RFIS to arrive.
    //
    if (!(p_port->is & PORT_IS_DHRS))
    {
        // RFIS did not arrive.
        //
        printf("ahci: command completed, but RFIS was not received\n");
        return (false);
    }

    // Clear RFIS interrupt flag.
    //
    p_port->is = PORT_IS_DHRS;

    // Check the error.
    //
    if (b_has_err)
    {
        printf("ahci: Error register is set to %x", g_rfis.rfis.error);
        if (g_rfis.rfis.error & SATA_ERROR_ABORT)
        {
            printf("; device aborted command");
        }
        printf("\n");
        return (false);
    }

    return (true);
}

static int
find_cmd_slot (reg_port_t * p_port)
{
    for (int slot = 0; slot < 32; slot++)
    {
        bool b_ds = (0 != (p_port->sact & (1 << slot)));  // device status
        bool b_ci = (0 != (p_port->ci & (1 << slot)));    // command issued
        if ((!b_ds) && (!b_ci))
        {
            return (slot);
        }
    }
    return (-1);
}

static void
dump_port_reg (reg_port_t * p_port)
{
    printf("Port register at %P:\n", p_port);
    printf("    clb = %08x\n", p_port->clb);
    printf("   clbu = %08x\n", p_port->clbu);
    printf("     fb = %08x\n", p_port->fb);
    printf("    fbu = %08x\n", p_port->fbu);
    printf("     is = %08x\n", p_port->is);
    printf("     ie = %08x\n", p_port->ie);
    printf("    cmd = %08x\n", p_port->cmd);
    printf("    tfd = %08x\n", p_port->tfd);
    printf("    sig = %08x\n", p_port->sig);
    printf("   ssts = %08x\n", p_port->ssts);
    printf("   sctl = %08x\n", p_port->sctl);
    printf("   serr = %08x\n", p_port->serr);
    printf("   sact = %08x\n", p_port->sact);
    printf("     ci = %08x\n", p_port->ci);
    printf("   sntf = %08x\n", p_port->sntf);
    printf("    fbs = %08x\n", p_port->fbs);
    printf(" devslp = %08x\n", p_port->devslp);
}
