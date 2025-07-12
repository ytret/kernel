/**
 * @file ahci_regs.h
 * AHCI register structure definitions.
 */

#pragma once

#include "blkdev/sata.h"
#include "types.h"

/**
 * Size of the HBA memory register map (bytes).
 * The spec (AHCI 1.3.1, section 3) says that the size is 0x1100, and that's for
 * 32 ports. However, QEMU implements only 6 ports and its memory map size is
 * 0x1000 bytes.
 */
#define AHCI_HBA_MAP_SIZE 0x1000

/**
 * Port @a P control register offset.
 * @param P Port number.
 */
#define AHCI_REG_PORT_OFFSET(P) (0x100 + (0x80 * (P)))

/**
 * Pointer to the port @a P control register.
 * @param HBA Start address of the HBA register map.
 * @param P   Port number.
 */
#define AHCI_REG_PORT(HBA, P)                                                  \
    ((reg_port_t *)((uint32_t)(HBA) + (AHCI_REG_PORT_OFFSET(P))))

/**
 * FIS Base Address alignment.
 * Refer to section 3.3.3, register PxFB (FIS Base Address) must be 256 byte
 * aligned.
 */
#define AHCI_FIS_BASE_ALIGN 256

/**
 * Command List alignment.
 * Refer to section 3.3.1, register PxCLB (Command List Base Address) must be
 * 1024 byte aligned.
 */
#define AHCI_CMD_LIST_ALIGN 1024

/**
 * Command List length.
 * Refer to section 4.2.2.
 */
#define AHCI_CMD_LIST_LEN 32

/**
 * Command Table alignment.
 * Refer to section 4.2.2, field CTBA0 has bits 0..6 reserved, so the address
 * must be 128 byte aligned.
 */
#define AHCI_CMD_TABLE_ALIGN 128

/**
 * Number of PRDT entries in each command table.
 * See #ahci_cmd_table_t.
 */
#define AHCI_CMD_TABLE_NUM_PRDS 8

/**
 * Generic HBA Control registers.
 * Refer to section 3.1.
 */
typedef volatile struct {
    IO32 cap;       //!< Host Capabilities register.
    IO32 ghc;       //!< Global HBA Control register.
    IO32 is;        //!< Interrupt Status register.
    IO32 pi;        //!< Ports Implemented register.
    IO32 vs;        //!< Version register.
    IO32 ccc_ctl;   //!< Command Completion Coalescing Control register.
    IO32 ccc_ports; //!< Command Completion Coalescing Ports register.
    IO32 em_loc;    //!< Enclosure Management Location register.
    IO32 em_ctl;    //!< Enclosure Management Control register.
    IO32 cap2;      //!< HBA Capabilities Extended register.
    IO32 bohc;      //!< BIOS/OS Handoff Control and Status register.
} reg_ghc_t;

/**
 * Supports AHCI Mode Only bit in the Host Capabilities register.
 * See #reg_ghc_t.cap, #ahci_cap_t.sam.
 */
#define AHCI_GHC_CAP_SAM (1 << 18)

/**
 * Host Capabilities register structure.
 * See #reg_ghc_t.cap.
 */
typedef struct {
    uint32_t np : 5;   //!< Number of Ports.
    uint32_t sxs : 1;  //!< Supports Extended SATA.
    uint32_t ems : 1;  //!< Enclosure Management Supported.
    uint32_t cccs : 1; //!< Command Completion Coalescing Supported.
    uint32_t ncs : 5;  //!< Number of Command Slots.
    uint32_t psc : 1;  //!< Partial State Capable.
    uint32_t ssc : 1;  //!< Slumber State Capable.
    uint32_t pmd : 1;  //!< PIO Multiple DRQ Block.
    uint32_t fbss : 1; //!< FIS-based Switching Supported.
    uint32_t spm : 1;  //!< Supports Port Multiplier.
    uint32_t sam : 1;  //!< Supports AHCI Mode Only.
    uint32_t reserved1 : 1;
    uint32_t iss : 4;   //!< Interface Speed Support.
    uint32_t sclo : 1;  //!< Supports Command List Override.
    uint32_t sal : 1;   //!< Supports Activity LED.
    uint32_t salp : 1;  //!< Supports Aggressive Link Power Management.
    uint32_t sss : 1;   //!< Supports Staggered Spin-up.
    uint32_t smps : 1;  //!< Supports Mechanical Presence Switch.
    uint32_t ssntf : 1; //!< Supports SNotification Register.
    uint32_t sncq : 1;  //!< Supports Native Command Queueing.
    uint32_t s64a : 1;  //!< Supports 64-bit Addressing.
} ahci_cap_t;

/**
 * Interface Speed Support (ISS) values in the Host Capabilities register.
 * See #ahci_cap_t.iss.
 */
typedef enum {
    AHCI_CAP_ISS_GEN1 = 0b0001, //!< Gen 1 (1.5 Gbps).
    AHCI_CAP_ISS_GEN2 = 0b0010, //!< Gen 2 (3 Gbps).
    AHCI_CAP_ISS_GEN3 = 0b0011, //!< Gen 3 (6 Gbps).
} ahci_cap_iss_t;

/**
 * AHCI Enable bit in the Global HBA Control register.
 * See #reg_ghc_t.ghc, #ahci_ghc_ghc_t.ae.
 */
#define AHCI_GHC_GHC_AE (1 << 31)

/**
 * Global HBA Control register structure.
 * See #reg_ghc_t.ghc.
 */
typedef struct {
    uint32_t hr : 1;   //!< HBA Reset.
    uint32_t ie : 1;   //!< Interrupt Enable.
    uint32_t mrsm : 1; //!< MSI Revert to Single Message.
    uint32_t reserved1 : 28;
    uint32_t ae : 1; //!< AHCI Enable.
} ahci_ghc_ghc_t;

/**
 * Version register structure.
 * See #reg_ghc_t.vs.
 */
typedef struct {
    uint32_t minor : 16;
    uint32_t major : 16;
} ahci_ghc_vs_t;

/**
 * AHCI Verson (VS) values in the Version register.
 * See #ahci_ghc_vs_t.
 */
typedef enum {
    AHCI_VERSION_0_95 = 0x00000905,  //!< AHCI 0.95 Compliant HBA.
    AHCI_VERSION_1_0 = 0x00010000,   //!< AHCI 1.0 Compliant HBA.
    AHCI_VERSION_1_1 = 0x00010100,   //!< AHCI 1.1 Compliant HBA.
    AHCI_VERSION_1_2 = 0x00010200,   //!< AHCI 1.2 Compliant HBA.
    AHCI_VERSION_1_3 = 0x00010300,   //!< AHCI 1.3 Compliant HBA.
    AHCI_VERSION_1_3_1 = 0x00010301, //!< AHCI 1.3.1 Compliant HBA.
} ahci_version_t;

/**
 * Command Completion Coalescing Control register structure.
 * See #reg_ghc_t.ccc_ctl.
 */
typedef struct {
    uint32_t en : 1; //!< Enable.
    uint32_t reserved1 : 2;
    uint32_t intr : 5; //!< Interrupt.
    uint32_t cc : 8;   //!< Command Completions.
    uint32_t tv : 16;  //!< Timeout Value.
} ahci_ghc_ccc_ctl_t;

/**
 * Enclosure Management Location register structure.
 * See #reg_ghc_t.em_loc.
 */
typedef struct {
    uint32_t sz : 16;   //!< Buffer Size.
    uint32_t ofst : 16; //!< Offset.
} ahci_ghc_em_loc_t;

/**
 * Enclosure Management Control register structure.
 * See #reg_ghc_t.em_ctl.
 */
typedef struct {
    uint32_t sts_mr : 1; //!< Message Received.
    uint32_t reserved1 : 7;
    uint32_t ctl_tm : 1;  //!< Transmit Message.
    uint32_t ctl_rst : 1; //!< Reset.
    uint32_t reserved2 : 6;
    uint32_t supp_led : 1;   //!< LED Message Types.
    uint32_t supp_safte : 1; //!< SAF-TE Enclosure Management Messages.
    uint32_t supp_ses2 : 1;  //!< SES-2 Enclosure Management Messages.
    uint32_t supp_sgpio : 1; //!< SGPIO Enclosure Management Messages.
    uint32_t reserved3 : 4;
    uint32_t attr_smb : 1;  //!< Single Message Buffer.
    uint32_t attr_xmt : 1;  //!< Transmit Only.
    uint32_t attr_alhd : 1; //!< Activity LED Hardware Driven.
    uint32_t attr_pm : 1;   //!< Port Multiplier Support.
    uint32_t reserved4 : 4;
} ahci_ghc_em_ctl_t;

/**
 * HBA Capabilities Extended register structure.
 * See #reg_ghc_t.cap2.
 */
typedef struct {
    uint32_t boh : 1;  //!< BIOS/OS Handoff.
    uint32_t nvmp : 1; //!< NVMHCI Present.
    uint32_t apst : 1; //!< Automatic Partial to Slumber Transitions.
    uint32_t sds : 1;  //!< Supports Device Sleep.
    uint32_t sadm : 1; //!< Supports Aggressive Device Sleep Management.
    uint32_t deso : 1; //!< DevSleep Entrance from Slumber Only.
    uint32_t reserved1 : 26;
} ahci_ghc_cap2_t;

/**
 * BIOS/OS Handoff Control and Status register structure.
 * See #reg_ghc_t.bohc.
 */
typedef struct {
    uint32_t bos : 1;  //!< BIOS Owned Semaphore.
    uint32_t oos : 1;  //!< OS Owned Semaphore.
    uint32_t sooe : 1; //!< SMI on OS Ownership Change Enable.
    uint32_t ooc : 1;  //!< OS Ownership Change.
    uint32_t bb : 1;   //!< BIOS Busy.
    uint32_t reserved1 : 27;
} ahci_ghc_bohc_t;

/**
 * Port registers.
 * Refer to section 3.3.
 */
typedef struct {
    IO32 clb;           //!< Command List Base Address register.
    IO32 clbu;          //!< Upper 32 Bits of the CLB register.
    IO32 fb;            //!< FIS Base Address register.
    IO32 fbu;           //!< Upper 32 Bits of the FB register.
    IO32 is;            //!< Interrupt Status register.
    IO32 ie;            //!< Interrupt Enable register.
    IO32 cmd;           //!< Command and Status register.
    IO32 _reserved_1;   //!< Resered.
    IO32 tfd;           //!< Task File Data register.
    IO32 sig;           //!< Signature register.
    IO32 ssts;          //!< SATA Status register.
    IO32 sctl;          //!< SATA Control register.
    IO32 serr;          //!< SATA Error register.
    IO32 sact;          //!< SATA Active register.
    IO32 ci;            //!< Command Issue register.
    IO32 sntf;          //!< SATA Notification register.
    IO32 fbs;           //!< FIS-based Switching Control register.
    IO32 devslp;        //!< Device Sleep register.
    IO32 reserved1[10]; //!< Reserved.
    IO32 p_vs[4];       //!< Vendor Specific registers.
} reg_port_t;

/**
 * Interrupt Enable (IE) and Interrupt Status (IS) bits.
 *
 * See #reg_port_t.is, #reg_port_t.ie.
 */
typedef enum {
    AHCI_PORT_INT_DHR = 1 << 0,  //!< Device to Host Register FIS Interrupt.
    AHCI_PORT_INT_PS = 1 << 1,   //!< PIO Setup FIS Interrupt.
    AHCI_PORT_INT_DS = 1 << 2,   //!< DMA Setup FIS Interrupt.
    AHCI_PORT_INT_SDB = 1 << 3,  //!< Set-Device-Bits FIS Interrupt.
    AHCI_PORT_INT_UF = 1 << 4,   //!< Unknown FIS Interrupt.
    AHCI_PORT_INT_DP = 1 << 5,   //!< Descriptor Processed Interrupt.
    AHCI_PORT_INT_PC = 1 << 6,   //!< Port Change Interrupt.
    AHCI_PORT_INT_DMP = 1 << 7,  //!< Device Mechanical Presence.
    AHCI_PORT_INT_PRC = 1 << 22, //!< PhyRdy Change Interrupt.
    AHCI_PORT_INT_IPM = 1 << 23, //!< Incorrect Port Multiplier.
    AHCI_PORT_INT_OF = 1 << 24,  //!< Overflow.
    AHCI_PORT_INT_INF = 1 << 26, //!< Interface Non-fatal Error.
    AHCI_PORT_INT_IF = 1 << 27,  //!< Interface Fatal Error.
    AHCI_PORT_INT_HBD = 1 << 28, //!< Host Bus Data Error.
    AHCI_PORT_INT_HBF = 1 << 29, //!< Host Bus Fatal Error.
    AHCI_PORT_INT_TFE = 1 << 30, //!< Task File Error.
    AHCI_PORT_INT_CPD = 1 << 31, //!< Cold Presence Detect.

    AHCI_PORT_INT_ALL = 0xFDC000FF, //!< Enable all interrupts.
} ahci_port_int_t;

/**
 * Start bit in the Command and Status register.
 * See #reg_port_t.cmd, #ahci_port_cmd_t.st.
 */
#define AHCI_PORT_CMD_ST  (1 << 0)
/**
 * FIS Receive Enable bit in the Command and Status register.
 * See #reg_port_t.cmd, #ahci_port_cmd_t.fre.
 */
#define AHCI_PORT_CMD_FRE (1 << 4)
/**
 * FIS Receive Running bit in the Command and Status register.
 * See #reg_port_t.cmd, #ahci_port_cmd_t.fr.
 */
#define AHCI_PORT_CMD_FR  (1 << 14)
/**
 * Command List Running bit in the Command and Status register.
 * See #reg_port_t.cmd, #ahci_port_cmd_t.cr.
 */
#define AHCI_PORT_CMD_CR  (1 << 15)

/**
 * Command and Status register structure.
 * See #reg_port_t.cmd.
 */
typedef struct {
    uint32_t st : 1;  //!< Start.
    uint32_t sud : 1; //!< Spin-Up Device.
    uint32_t pod : 1; //!< Power On Device.
    uint32_t clo : 1; //!< Command List Override.
    uint32_t fre : 1; //!< FIS Receive Enable.
    uint32_t reserved1 : 3;
    uint32_t ccs : 5;   //!< Current Command Slot.
    uint32_t mpss : 1;  //!< Mechanical Presence Switch State.
    uint32_t fr : 1;    //!< FIS Receive Running.
    uint32_t cr : 1;    //!< Command List Running.
    uint32_t cps : 1;   //!< Cold Presence State.
    uint32_t pma : 1;   //!< Port Multiplier Attached.
    uint32_t hpcp : 1;  //!< Hot Plug Capable Port.
    uint32_t mpsp : 1;  //!< Mechanical Presence Switch Attached to Port.
    uint32_t cpd : 1;   //!< Cold Presence Detection.
    uint32_t esp : 1;   //!< External SATA Port.
    uint32_t fbscp : 1; //!< FIS-based Switching Capable Port.
    uint32_t apste : 1; //!< Auto Partial to Slumber Transitions Enabled.
    uint32_t atapi : 1; //!< Device is ATAPI.
    uint32_t dlae : 1;  //!< Drive LED on ATAPI Enable.
    uint32_t alpe : 1;  //!< Aggressive Link Power Management Enable.
    uint32_t asp : 1;   //!< Aggressive Slumber / Partial.
    uint32_t icc : 4;   //!< Interface Communication Control.
} ahci_port_cmd_t;

/**
 * Interface Communication Control (ICC) values in the Port Command register.
 * See #ahci_port_cmd_t.icc.
 */
typedef enum {
    AHCI_CMD_ICC_IDLE = 0x00,     //!< No-Op / Idle.
    AHCI_CMD_ICC_ACTIVE = 0x01,   //!< Transition into the Active state.
    AHCI_CMD_ICC_PARTIAL = 0x02,  //!< Transition into the Partial state.
    AHCI_CMD_ICC_SLUMBER = 0x06,  //!< Transition into the Slumber state.
    AHCI_CMD_ICC_DEVSLEEP = 0x08, //!< Transition into the DevSleep state.
} ahci_port_cmd_icc_t;

/**
 * Task File Data register structure.
 * See #reg_port_t.tfd.
 */
typedef struct {
    uint32_t sts : 8; //!< Status.
    uint32_t err : 8; //!< Error.
    uint32_t reserved1 : 16;
} ahci_port_tfd_t;

/**
 * Status (STS) values in the Port Task File Data register.
 * See #ahci_port_tfd_t.sts.
 */
typedef enum {
    AHCI_TFD_STS_ERR = 0x00, //!< An error during transfer.
    AHCI_TFD_STS_DRQ = 0x08, //!< A data transfer is requested.
    AHCI_TFD_STS_BSY = 0x80, //!< The interface is busy.
} ahci_port_tfd_sts_t;

/**
 * SATA Status register structure.
 * See #reg_port_t.ssts.
 */
typedef struct {
    uint32_t det : 4; //!< Device Detection.
    uint32_t spd : 4; //!< Current Interface Speed.
    uint32_t ipm : 4; //!< Interface Power Management.
} ahci_port_ssts_t;

/**
 * Device Detection (DET) values in the Port SATA Status register.
 * See #ahci_port_ssts_t.det.
 */
typedef enum {
    AHCI_SSTS_DET_NDEV_NPHY = 0x00, //!< No device, no Phy communication.
    AHCI_SSTS_DET_DEV_NPHY = 0x01,  //!< Device detected, no Phy communication.
    AHCI_SSTS_DET_DEV_PHY = 0x03,   //!< Device detected, Phy established.
    AHCI_SSTS_DET_PHY_OFF = 0x04,   //!< Phy in offline mode.
} ahci_port_ssts_det_t;

/**
 * SATA Control register structure.
 * See #reg_port_t.sctl.
 */
typedef struct {
    uint32_t det : 4; //!< Device Detection Initialization.
    uint32_t spd : 4; //!< Speed Allowed.
    uint32_t ipm : 4; //!< Interface Power Management Transitions Allowed.
    uint32_t reserved1 : 20;
} ahci_port_sctl_t;

/**
 * Device Detection (DET) values in the Port SATA Control register.
 * See #ahci_port_sctl_t.det.
 */
typedef enum {
    /// No request.
    AHCI_SCTL_DET_NONE = 0x00,
    /**
     * Hard reset the communication interface.
     * Set to this value for at least 1 ms in order for the COMRESET to be
     * transmitted.
     */
    AHCI_SCTL_DET_RESET = 0x01,
    /// Disable the SATA interface and put PHY in offline mode.
    AHCI_SCTL_DET_OFF = 0x04,
} ahci_port_sctl_det_t;

/**
 * Speed Allowed (SPD) values in the Port SATA Control register.
 * See #ahci_port_sctl_t.spd, #ahci_cap_iss_t.
 */
typedef enum {
    AHCI_SCTL_SPD_NONE = 0x00, //!< No speed negotiation restrictions.
    AHCI_SCTL_SPD_GEN1 = 0x01, //!< Limit speed negotiation to Gen1.
    AHCI_SCTL_SPD_GEN2 = 0x02, //!< Limit speed negotiation up to Gen2.
    AHCI_SCTL_SPD_GEN3 = 0x03, //!< Limit speed negotiation up to Gen3.
} ahci_port_sctl_spd_t;

/**
 * Interface Power Management Transitions Allowed (IPM) bits in Port SCTL.
 * OR this values to disable transition into several states.
 * See #ahci_port_sctl_t.spd.
 */
typedef enum {
    /// No interface restrictions.
    AHCI_SCTL_IPM_NONE = 0x00,
    /// Transition to Partial state disabled.
    AHCI_SCTL_IPM_DIS_PARTIAL = 0x01,
    /// Transition to Slumber state disabled.
    AHCI_SCTL_IPM_DIS_SLUMBER = 0x02,
    /// Transition to DevSleep state disabled.
    AHCI_SCTL_IPM_DIS_DEVSLEEP = 0x04,
} ahci_port_sctl_ipm_t;

/**
 * SATA Error register structure.
 * See #reg_port_t.serr.
 */
typedef struct {
    uint32_t diag : 16; //!< Diagnostics.
    uint32_t err : 16;  //!< Error.
} ahci_port_serr_t;

/**
 * Diagnostics (DIAG) values in the Port SATA Error register.
 * See #ahci_port_serr_t.diag.
 */
typedef enum {
    /// PhyRdy signal changed state.
    AHCI_SERR_DIAG_PHYRDY_CHANGE = 0x0001,
    /// Phy detected some internal error.
    AHCI_SERR_DIAG_PHY_INTERNAL = 0x0002,
    /// Phy detected a Comm Wake signal.
    AHCI_SERR_DIAG_COMM_WAKE = 0x0004,
    /// One or more 10B to 8B decoding errors.
    AHCI_SERR_DIAG_10B_TO_8B_DECODE = 0x0008,
    // AHCI_SERR_DIAG_DISPARITY = 0x0010, - not used by AHCI
    /// One or more CRC errors within the Link Layer.
    AHCI_SERR_DIAG_CRC = 0x0020,
    /// One or more error responses to a frame tranmission.
    AHCI_SERR_DIAG_HANDSHAKE = 0x0040,
    /// Erroneous Link Layer state machine transition.
    AHCI_SERR_DIAG_LINK_STATE = 0x0080,
    /// Erroneous Transport Layer state machine transition.
    AHCI_SERR_DIAG_TRANSPORT_STATE = 0x0100,
    /// Transport Layer recevied an unrecognized FIS type.
    AHCI_SERR_DIAG_UNKNOWN_FIS_TYPE = 0x0200,
    /// Device presence change has been detected.
    AHCI_SERR_DIAG_EXCHANGED = 0x0400,
} ahci_port_serr_diag_t;

/**
 * FIS-based Switching Control register structure.
 * See #reg_port_t.fbs.
 */
typedef struct {
    uint32_t en : 1;  //!< Enable.
    uint32_t dec : 1; //!< Device Error Clear.
    uint32_t sde : 1; //!< Single Device Error.
    uint32_t reserved1 : 5;
    uint32_t dev : 4; //!< Device To Issue.
    uint32_t ado : 4; //!< Active Device Optimization.
    uint32_t dwe : 4; //!< Device With Error.
    uint32_t reserved2 : 12;
} ahci_port_fbs_t;

/**
 * Device Sleep register.
 */
typedef struct {
    uint32_t adse : 1;  //!< Aggressive Device Sleep Enable.
    uint32_t dsp : 1;   //!< Device Sleep Present.
    uint32_t deto : 8;  //!< Device Sleep Exit Timeout.
    uint32_t mdat : 5;  //!< Minimum Device Sleep Assertion Time.
    uint32_t dito : 10; //!< Device Sleep Idle Timeout.
    uint32_t dm : 3;    //!< DITO (Device Sleep Idle Timeout) Multiplier.
    uint32_t reserved1 : 3;
} ahci_port_devslp_t;

/**
 * Error (ERR) values in the Port SATA Error Register.
 * See #ahci_port_serr_t.err.
 */
typedef enum {
    /// Data integrity error occurred and recovered from.
    AHCI_SERR_ERR_RECOVERED_INTEGRITY = 0x0001,
    /// Communication temporarily lost and recovered from.
    AHCI_SERR_ERR_RECOVERED_COMM = 0x0002,
    /// Data integrity error occurred, but not recovered from
    AHCI_SERR_ERR_TRANSIENT_INTEGRITY = 0x0100,
    /// Persistent communication error.
    AHCI_SERR_ERR_PERSISTENT_COMM = 0x0200,
    /// Violation of the SATA protocol detected.
    AHCI_SERR_ERR_PROTOCOL = 0x0400,
    /// Host Bus Adapter internal error.
    AHCI_SERR_ERR_INTERNAL = 0x0800,
} ahci_port_serr_err_t;

/**
 * Received Frame Information Structure (FIS).
 *
 * Frames that are received from the host are copied to the appropriate field of
 * this structure by the HBA.
 *
 * Refer to section 4.2.1.
 */
typedef volatile struct [[gnu::packed]] {
    /// DMA Setup FIS.
    sata_fis_dma_setup_t dsfis [[gnu::aligned(0x20)]];
    /// Port IO Setup FIS.
    sata_fis_pio_setup_t psfis [[gnu::aligned(0x20)]];
    /// Device-to-HBA FIS.
    sata_fis_reg_d2h_t rfis [[gnu::aligned(0x20)]];
    /// Set-Device-Bits FIS.
    sata_fis_dev_bits_t sdbfis [[gnu::aligned(0x08)]];

    /// Unknown FIS.
    uint8_t p_ufis[64];
    uint8_t _reserved[96];
} ahci_rfis_t;

/**
 * Command Header.
 * Refer to section 4.2.2.
 */
typedef volatile struct [[gnu::packed]] {
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
typedef volatile struct [[gnu::packed]] {
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
typedef volatile struct [[gnu::packed, gnu::aligned(128)]] {
    uint8_t p_cfis[64]; //!< Command FIS.
    uint8_t p_acmd[16]; //!< ATAPI Command.
    uint8_t _reserved[48];

    /**
     * Physical Region Descriptor Table (PRDT).
     * The number of items in this table is set by the #ahci_cmd_hdr_t.prdtl
     * field in the Command Header.
     */
    ahci_prd_t p_prd_table[AHCI_CMD_TABLE_NUM_PRDS];
} ahci_cmd_table_t;
