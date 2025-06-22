/**
 * @file ahci_regs.h
 * AHCI register structure definitions.
 */

#pragma once

#include "disk/sata.h"
#include "types.h"

/// Size of the HBA memory register map (bytes).
#define AHCI_HBA_REGS_SIZE 0x1100

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
typedef volatile struct [[gnu::packed]] {
    /// Host Capabilities register.
    union {
        IO32 cap;
        struct {
            IO32 np : 5;   //!< Number of Ports.
            IO32 sxs : 1;  //!< Supports Extended SATA.
            IO32 ems : 1;  //!< Enclosure Management Supported.
            IO32 cccs : 1; //!< Command Completion Coalescing Supported.
            IO32 ncs : 5;  //!< Number of Command Slots.
            IO32 psc : 1;  //!< Partial State Capable.
            IO32 ssc : 1;  //!< Slumber State Capable.
            IO32 pmd : 1;  //!< PIO Multiple DRQ Block.
            IO32 fbss : 1; //!< FIS-based Switching Supported.
            IO32 spm : 1;  //!< Supports Port Multiplier.
            IO32 sam : 1;  //!< Supports AHCI Mode Only.
            IO32 reserved1 : 1;
            IO32 iss : 4;   //!< Interface Speed Support.
            IO32 sclo : 1;  //!< Supports Command List Override.
            IO32 sal : 1;   //!< Supports Activity LED.
            IO32 salp : 1;  //!< Supports Aggressive Link Power Management.
            IO32 sss : 1;   //!< Supports Staggered Spin-up.
            IO32 smps : 1;  //!< Supports Mechanical Presence Switch.
            IO32 ssntf : 1; //!< Supports SNotification Register.
            IO32 sncq : 1;  //!< Supports Native Command Queueing.
            IO32 s64a : 1;  //!< Supports 64-bit Addressing.
        } cap_bit;
    };

    /// Global HBA Control register.
    union {
        IO32 ghc;
        struct {
            IO32 hr : 1;   //!< HBA Reset.
            IO32 ie : 1;   //!< Interrupt Enable.
            IO32 mrsm : 1; //!< MSI Revert to Single Message.
            IO32 reserved1 : 28;
            IO32 ae : 1; //!< AHCI Enable.
        } ghc_bit;
    };
    IO32 is; //!< Interrupt Status register.
    IO32 pi; //!< Ports Implemented register.

    /// Version register.
    union {
        IO32 vs;
        struct {
            IO32 minor : 16;
            IO32 major : 16;
        } vs_bit;
    };

    /// Command Completion Coalescing Control register.
    union {
        IO32 ccc_ctl;
        struct {
            IO32 en : 1; //!< Enable.
            IO32 reserved1 : 2;
            IO32 intr : 5; //!< Interrupt.
            IO32 cc : 8;   //!< Command Completions.
            IO32 tv : 16;  //!< Timeout Value.
        } ccc_ctl_bit;
    };

    IO32 ccc_ports; //!< Command Completion Coalescing Ports register.

    /// Enclosure Management Location register.
    union {
        IO32 em_loc;
        struct {
            IO32 sz : 16;   //!< Buffer Size.
            IO32 ofst : 16; //!< Offset.
        } em_loc_bit;
    };

    /// Enclosure Management Control register.
    union {
        IO32 em_ctl;
        struct {
            IO32 sts_mr : 1; //!< Message Received.
            IO32 reserved1 : 7;
            IO32 ctl_tm : 1;  //!< Transmit Message.
            IO32 ctl_rst : 1; //!< Reset.
            IO32 reserved2 : 6;
            IO32 supp_led : 1;   //!< LED Message Types.
            IO32 supp_safte : 1; //!< SAF-TE Enclosure Management Messages.
            IO32 supp_ses2 : 1;  //!< SES-2 Enclosure Management Messages.
            IO32 supp_sgpio : 1; //!< SGPIO Enclosure Management Messages.
            IO32 reserved3 : 4;
            IO32 attr_smb : 1;  //!< Single Message Buffer.
            IO32 attr_xmt : 1;  //!< Transmit Only.
            IO32 attr_alhd : 1; //!< Activity LED Hardware Driven.
            IO32 attr_pm : 1;   //!< Port Multiplier Support.
            IO32 reserved4 : 4;
        } em_ctl_bit;
    };

    /// HBA Capabilities Extended register.
    union {
        IO32 cap2;
        struct {
            IO32 boh : 1;  //!< BIOS/OS Handoff.
            IO32 nvmp : 1; //!< NVMHCI Present.
            IO32 apst : 1; //!< Automatic Partial to Slumber Transitions.
            IO32 sds : 1;  //!< Supports Device Sleep.
            IO32 sadm : 1; //!< Supports Aggressive Device Sleep Management.
            IO32 deso : 1; //!< DevSleep Entrance from Slumber Only.
            IO32 reserved1 : 26;
        } cap2_bit;
    };

    /// BIOS/OS Handoff Control and Status register.
    union {
        IO32 bohc;
        struct {
            IO32 bos : 1;  //!< BIOS Owned Semaphore.
            IO32 oos : 1;  //!< OS Owned Semaphore.
            IO32 sooe : 1; //!< SMI on OS Ownership Change Enable.
            IO32 ooc : 1;  //!< OS Ownership Change.
            IO32 bb : 1;   //!< BIOS Busy.
            IO32 reserved1 : 27;
        } bohc_bit;
    };
} reg_ghc_t;

/// Interface Speed Support (ISS) values in the Host Capabilities register.
typedef enum {
    AHCI_CAP_ISS_GEN1 = 0b0001, //!< Gen 1 (1.5 Gbps).
    AHCI_CAP_ISS_GEN2 = 0b0010, //!< Gen 2 (3 Gbps).
    AHCI_CAP_ISS_GEN3 = 0b0011, //!< Gen 3 (6 Gbps).
} ahci_cap_iss_t;

/// AHCI Verson (VS) values in the Host Capabilities register.
typedef enum {
    AHCI_VERSION_0_95 = 0x00000905,  //!< AHCI 0.95 Compliant HBA.
    AHCI_VERSION_1_0 = 0x00010000,   //!< AHCI 1.0 Compliant HBA.
    AHCI_VERSION_1_1 = 0x00010100,   //!< AHCI 1.1 Compliant HBA.
    AHCI_VERSION_1_2 = 0x00010200,   //!< AHCI 1.2 Compliant HBA.
    AHCI_VERSION_1_3 = 0x00010300,   //!< AHCI 1.3 Compliant HBA.
    AHCI_VERSION_1_3_1 = 0x00010301, //!< AHCI 1.3.1 Compliant HBA.
} ahci_version_t;

/**
 * Port registers.
 * Refer to section 3.3.
 */
typedef struct [[gnu::packed]] {
    IO32 clb;  //!< Command List Base Address register.
    IO32 clbu; //!< Upper 32 Bits of the CLB register.
    IO32 fb;   //!< FIS Base Address register.
    IO32 fbu;  //!< Upper 32 Bits of the FB register.

    /// Interrupt Status register.
    union {
        IO32 is;
        struct {
            IO32 dhrs : 1; //!< Device to Host Register FIS Interrupt.
            IO32 pss : 1;  //!< PIO Setup FIS Interrupt.
            IO32 dss : 1;  //!< DMA Setup FIS Interrupt.
            IO32 sdbs : 1; //!< Set-Device-Bits Interrupt.
            IO32 ufs : 1;  //!< Unknown FIS Interrupt.
            IO32 dps : 1;  //!< Descriptor Processed.
            IO32 pcs : 1;  //!< Port Connect Change Status.
            IO32 dmps : 1; //!< Device Mechanical Presence Status.
            IO32 reserved1 : 14;
            IO32 prcs : 1; //!< PhyRdy Change Status.
            IO32 ipms : 1; //!< Incorrect Port Multiplier Status.
            IO32 ofs : 1;  //!< Overflow Status.
            IO32 reserved2 : 1;
            IO32 infs : 1; //!< Interface Non-fatal Error Status.
            IO32 ifs : 1;  //!< Interface Fatal Error Status.
            IO32 hbds : 1; //!< Host Bus Data Error Status.
            IO32 hbfs : 1; //!< Host Bus Fatal Error Status.
            IO32 tfes : 1; //!< Task File Error Status.
            IO32 cpds : 1; //!< Cold Port Detect Status.
        } is_bit;
    };

    /// Interrupt Enable register.
    union {
        IO32 ie;
        struct {
            IO32 dhre : 1; //!< Device to Host Register FIS Interrupt Enable.
            IO32 pse : 1;  //!< PIO Setup FIS Interrupt Enable.
            IO32 dse : 1;  //!< DMA Setup FIS Interrupt Enable.
            IO32 sdbe : 1; //!< Set-Device-Bits FIS Interrupt Enable.
            IO32 ufe : 1;  //!< Unknown FIS Interrupt Enable.
            IO32 dpe : 1;  //!< Descriptor Processed Interrupt Enable.
            IO32 pce : 1;  //!< Port Change Interrupt Enable.
            IO32 dmpe : 1; //!< Device Mechanical Presence Enable.
            IO32 reserved1 : 14;
            IO32 prce : 1; //!< PhyRdy Change Interrupt Enable.
            IO32 ipme : 1; //!< Incorrect Port Multiplier Enable.
            IO32 ofe : 1;  //!< Overflow Enable.
            IO32 reserved2 : 1;
            IO32 infe : 1; //!< Interface Non-fatal Error Enable.
            IO32 ife : 1;  //!< Interface Fatal Error Enable.
            IO32 hbde : 1; //!< Host Bus Data Error Enable.
            IO32 hbfe : 1; //!< Host Bus Fatal Error Enable.
            IO32 tfee : 1; //!< Task File Error Enable.
            IO32 cpde : 1; //!< Cold Presence Detect Enable.
        } ie_bit;
    };

    /// Command and Status register.
    union {
        IO32 cmd;
        struct {
            IO32 st : 1;  //!< Start.
            IO32 sud : 1; //!< Spin-Up Device.
            IO32 pod : 1; //!< Power On Device.
            IO32 clo : 1; //!< Command List Override.
            IO32 fre : 1; //!< FIS Receive Enable.
            IO32 reserved1 : 3;
            IO32 ccs : 5;   //!< Current Command Slot.
            IO32 mpss : 1;  //!< Mechanical Presence Switch State.
            IO32 fr : 1;    //!< FIS Receive Running.
            IO32 cr : 1;    //!< Command List Running.
            IO32 cps : 1;   //!< Cold Presence State.
            IO32 pma : 1;   //!< Port Multiplier Attached.
            IO32 hpcp : 1;  //!< Hot Plug Capable Port.
            IO32 mpsp : 1;  //!< Mechanical Presence Switch Attached to Port.
            IO32 cpd : 1;   //!< Cold Presence Detection.
            IO32 esp : 1;   //!< External SATA Port.
            IO32 fbscp : 1; //!< FIS-based Switching Capable Port.
            IO32 apste : 1; //!< Auto Partial to Slumber Transitions Enabled.
            IO32 atapi : 1; //!< Device is ATAPI.
            IO32 dlae : 1;  //!< Drive LED on ATAPI Enable.
            IO32 alpe : 1;  //!< Aggressive Link Power Management Enable.
            IO32 asp : 1;   //!< Aggressive Slumber / Partial.
            IO32 icc : 4;   //!< Interface Communication Control.
        } cmd_bit;
    };

    IO32 _reserved_1; //!< Reserved.

    /// Task File Data register.
    union {
        IO32 tfd;
        struct {
            IO32 sts : 8; //!< Status.
            IO32 err : 8; //!< Error.
            IO32 reserved1 : 16;
        } tfd_bit;
    };

    IO32 sig; //!< Signature register.

    /// SATA Status register.
    union {
        IO32 ssts;
        struct {
            IO32 det : 4; //!< Device Detection.
            IO32 spd : 4; //!< Current Interface Speed.
            IO32 ipm : 4; //!< Interface Power Management.
        } ssts_bit;
    };

    /// SATA Control register.
    union {
        IO32 sctl;
        struct {
            IO32 det : 4; //!< Device Detection Initialization.
            IO32 spd : 4; //!< Speed Allowed.
            IO32 ipm : 4; //!< Interface Power Management Transitions Allowed.
            IO32 reserved1 : 20;
        } sctl_bit;
    };

    /// SATA Error register.
    union {
        IO32 serr;
        struct {
            IO32 diag : 16; //!< Diagnostics.
            IO32 err : 16;  //!< Error.
        } serr_bit;
    };

    IO32 sact; //!< SATA Active register.
    IO32 ci;   //!< Command Issue register.
    IO32 sntf; //!< SATA Notification register.

    /// FIS-based Switching Control register.
    union {
        IO32 fbs;
        struct {
            IO32 en : 1;  //!< Enable.
            IO32 dec : 1; //!< Device Error Clear.
            IO32 sde : 1; //!< Single Device Error.
            IO32 reserved1 : 5;
            IO32 dev : 4; //!< Device To Issue.
            IO32 ado : 4; //!< Active Device Optimization.
            IO32 dwe : 4; //!< Device With Error.
            IO32 reserved2 : 12;
        } fbs_bit;
    };

    /// Device Sleep register.
    union {
        IO32 devslp;
        struct {
            IO32 adse : 1;  //!< Aggressive Device Sleep Enable.
            IO32 dsp : 1;   //!< Device Sleep Present.
            IO32 deto : 8;  //!< Device Sleep Exit Timeout.
            IO32 mdat : 5;  //!< Minimum Device Sleep Assertion Time.
            IO32 dito : 10; //!< Device Sleep Idle Timeout.
            IO32 dm : 3;    //!< DITO (Device Sleep Idle Timeout) Multiplier.
            IO32 reserved1 : 3;
        } devslp_bit;
    };

    IO32 reserved1[10]; //!< Reserved.
    IO32 p_vs[4];       //!< Vendor Specific registers.
} reg_port_t;

/// Interface Communication Control (ICC) values in the Port Command register.
typedef enum {
    AHCI_CMD_ICC_IDLE = 0x00,     //!< No-Op / Idle.
    AHCI_CMD_ICC_ACTIVE = 0x01,   //!< Transition into the Active state.
    AHCI_CMD_ICC_PARTIAL = 0x02,  //!< Transition into the Partial state.
    AHCI_CMD_ICC_SLUMBER = 0x06,  //!< Transition into the Slumber state.
    AHCI_CMD_ICC_DEVSLEEP = 0x08, //!< Transition into the DevSleep state.
} ahci_port_cmd_icc_t;

/// Status (STS) values in the Port Task File Data register.
typedef enum {
    AHCI_TFD_STS_ERR = 0x00, //!< An error during transfer.
    AHCI_TFD_STS_DRQ = 0x08, //!< A data transfer is requested.
    AHCI_TFD_STS_BSY = 0x80, //!< The interface is busy.
} ahci_port_tfd_sts_t;

/// Device Detection (DET) values in the Port SATA Status register.
typedef enum {
    AHCI_SSTS_DET_NDEV_NPHY = 0x00, //!< No device, no Phy communication.
    AHCI_SSTS_DET_DEV_NPHY = 0x01,  //!< Device detected, no Phy communication.
    AHCI_SSTS_DET_DEV_PHY = 0x03,   //!< Device detected, Phy established.
    AHCI_SSTS_DET_PHY_OFF = 0x04,   //!< Phy in offline mode.
} ahci_port_ssts_det_t;

/// Device Detection (DET) values in the Port SATA Control register.
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
 * See #ahci_cap_iss_t.
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

/// Diagnostics (DIAG) values in the Port SATA Error register.
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

/// Error (ERR) values in the Port SATA Error Register.
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
