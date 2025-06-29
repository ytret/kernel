/**
 * @file apic_regs.h
 * Advanced Programmable Interrupt Controller (APIC) register definitions.
 *
 * For Local APIC, refer to Intel(R) 64 and IA-32 Architectures Software
 * Developer's Manual, Volume 3 (3A, 3B, 3C & 3D): System Programming Guide,
 * December 2021 (later revisions have different chapter numbers).
 *
 * For I/O APIC, refer to 82093AA I/O Advanced Programmable Interrupt Controller
 * (IOAPIC) datasheet, May 1996.
 */

#pragma once

#include "types.h"

/**
 * I/O APIC first Redirection Table Entry register number.
 * See #ioapic_regs_t and #ioapic_redir_t.
 */
#define IOAPIC_REG_REDIR(x) (0x10 + 2 * (x))

/**
 * Local APIC registers.
 * Refer to section 10.4.1, "The Local APIC Block Diagram", and Table 10-1
 * "Local APIC Register Address Map".
 */
typedef volatile struct [[gnu::packed]] {
    IO8 reserved1[32];

    /// Local APIC ID (r/w).
    union [[gnu::packed]] {
        IO32 lapic_id;
        struct [[gnu::packed]] {
            IO32 reserved1 : 24;
            IO32 apic_id : 4;
            IO32 reserved2 : 4;
        } lapic_id_bit;
    };

    IO8 reserved2[12];

    /// Local APIC Version (r/o).
    union [[gnu::packed]] {
        IO32 lapic_version;
        struct [[gnu::packed]] {
            IO32 version : 8;
            IO32 reserved1 : 8;
            IO32 max_lvt_entry : 8;
            IO32 eoi_broadcast_suppr : 1;
            IO32 reserved2 : 7;
        } lapic_version_bit;
    };

    IO8 reserved3[76];

    ALIGN(16) IO32 tpr; //!< Task Priority (r/w).
    ALIGN(16) IO32 apr; //!< Arbitration Priority (r/o).
    ALIGN(16) IO32 ppr; //!< Processor Priority (r/o).
    ALIGN(16) IO32 eoi; //!< End of Interrupt (w/o).
    ALIGN(16) IO32 rrd; //!< Remote Read (r/o).
    ALIGN(16) IO32 ldr; //!< Logical Destination (r/w).
    ALIGN(16) IO32 dfr; //!< Destination Format (r/w).

    IO8 reserved4[12];

    /// Spurious Interrupt Vector (r/w).
    union [[gnu::packed]] {
        IO32 sivr;
        struct [[gnu::packed]] {
            IO32 sv : 8;  //!< Spurious Vector.
            IO32 ase : 1; //!< APIC Software Enable/Disable.
            IO32 fpc : 1; //!< Focus Processor Checking.
            IO32 reserved1 : 2;
            IO32 ebs : 1; //!< EOI-Broadcast Suppression.
            IO32 reserved2 : 19;
        } sivr_bit;
    };

    ALIGN(16) IO32 isr_31_0;    //!< In-Service, bits 31..0 (r/o).
    ALIGN(16) IO32 isr_63_32;   //!< In-Service, bits 63..32 (r/o).
    ALIGN(16) IO32 isr_95_64;   //!< In-Service, bits 95..64 (r/o).
    ALIGN(16) IO32 isr_127_96;  //!< In-Service, bits 127..96 (r/o).
    ALIGN(16) IO32 isr_159_128; //!< In-Service, bits 159..128 (r/o).
    ALIGN(16) IO32 isr_191_160; //!< In-Service, bits 191..160 (r/o).
    ALIGN(16) IO32 isr_223_192; //!< In-Service, bits 223..192 (r/o).
    ALIGN(16) IO32 isr_255_224; //!< In-Service, bits 255..224 (r/o).

    ALIGN(16) IO32 tmr_31_0;    //!< Trigger Mode, bits 31..0 (r/o).
    ALIGN(16) IO32 tmr_63_32;   //!< Trigger Mode, bits 63..32 (r/o).
    ALIGN(16) IO32 tmr_95_64;   //!< Trigger Mode, bits 95..64 (r/o).
    ALIGN(16) IO32 tmr_127_96;  //!< Trigger Mode, bits 127..96 (r/o).
    ALIGN(16) IO32 tmr_159_128; //!< Trigger Mode, bits 159..128 (r/o).
    ALIGN(16) IO32 tmr_191_160; //!< Trigger Mode, bits 191..160 (r/o).
    ALIGN(16) IO32 tmr_223_192; //!< Trigger Mode, bits 223..192 (r/o).
    ALIGN(16) IO32 tmr_255_224; //!< Trigger Mode, bits 255..224 (r/o).

    ALIGN(16) IO32 irr_31_0;    //!< Interrupt Request, bits 31..0 (r/o).
    ALIGN(16) IO32 irr_63_32;   //!< Interrupt Request, bits 63..32 (r/o).
    ALIGN(16) IO32 irr_95_64;   //!< Interrupt Request, bits 95..64 (r/o).
    ALIGN(16) IO32 irr_127_96;  //!< Interrupt Request, bits 127..96 (r/o).
    ALIGN(16) IO32 irr_159_128; //!< Interrupt Request, bits 159..128 (r/o).
    ALIGN(16) IO32 irr_191_160; //!< Interrupt Request, bits 191..160 (r/o).
    ALIGN(16) IO32 irr_223_192; //!< Interrupt Request, bits 223..192 (r/o).
    ALIGN(16) IO32 irr_255_224; //!< Interrupt Request, bits 255..224 (r/o).

    ALIGN(16) IO32 esr; //!< Error Status (r/o).

    IO8 reserved5[108];

    ALIGN(16) IO32 cmci;      //!< LVT Corrected Machine Check Interrupt (r/w).
    ALIGN(16) IO32 icr_31_0;  //!< Interrupt Command, bits 31..0 (r/w).
    ALIGN(16) IO32 icr_63_32; //!< Interrupt Command, bits 63..32 (r/w).
    ALIGN(16) IO32 lvt_tim;   //!< LVT Timer (r/w).
    ALIGN(16) IO32 lvt_thm;   //!< LVT Thermal Sensor (r/w).
    ALIGN(16) IO32 lvt_pmc;   //!< LVT Performance Monitoring Counters (r/w).
    ALIGN(16) IO32 lvt_lint0; //!< LVT LINT0 (r/w).
    ALIGN(16) IO32 lvt_lint1; //!< LVT LINT1 (r/w).
    ALIGN(16) IO32 lvt_err;   //!< LVT Error (r/w).

    ALIGN(16) IO32 icr; //!< Initial Count (for Timer) (r/w).
    ALIGN(16) IO32 ccr; //!< Current Count (for Timer) (r/o).
    IO8 reserved6[76];
    ALIGN(16) IO32 dcr; //!< Divide Configuration (for Timer) (r/w).
    IO8 reserved7[16];
} lapic_regs_t;

/**
 * I/O APIC Register I/O interface.
 * Refer to section 3.1.
 */
typedef volatile struct [[gnu::packed]] {
    ALIGN(16) IO32 regsel;
    ALIGN(16) IO32 win;
} ioapic_regs_t;

/**
 * Some I/O APIC registers.
 * See #IOAPIC_REG_REDIR().
 * Refer to section 3.2.
 */
typedef enum {
    IOAPIC_REG_ID = 0x00,      //!< IOAPIC Identification register.
    IOAPIC_REG_VERSION = 0x01, //!< IOAPIC Version Register.
    IOAPIC_REG_ARBITR = 0x02, //!< IOAPIC Arbitration Register.
} ioapic_reg_sel_t;

/**
 * I/O APIC Identification Register structure.
 * Refer to section 3.2.1.
 */
typedef struct [[gnu::packed]] {
    uint32_t reserved1 : 24;
    uint32_t ioapic_id : 4;
    uint32_t reserved2 : 4;
} ioapic_reg_id_t;

/**
 * I/O APIC Version Register structure.
 * Refer to section 3.2.2.
 */
typedef struct [[gnu::packed]] {
    uint32_t version : 8;
    uint32_t reserved : 8;
    uint32_t max_redir_entry : 8;
    uint32_t reserved2 : 8;
} ioapic_reg_ver_t;

/**
 * I/O APIC Redirecton Table Entry structure.
 * Refer to section 3.2.4.
 * See #IOAPIC_REG_REDIR().
 */
typedef struct [[gnu::packed]] {
    uint64_t intvec : 8;  //<! Interrupt Vector 0x10..0xFE (r/w).
    uint64_t delmod : 3;  //!< Delivery Mode (r/w).
    uint64_t destmod : 1; //!< Destination Mode (r/w).
    uint64_t delivs : 1;  //!< Delivery Status (r/o).
    uint64_t intpol : 1;  //!< Interrupt Input Pin Polarity (r/w).
    uint64_t remirr : 1;  //!< Remote IRR (r/o).
    uint64_t trigmod : 1; //!< Trigger Mode (r/w).
    uint64_t intmask : 1; //!< Interrupt Mask (r/w).
    uint64_t reserved1 : 39;

    /// Destination Field union tagged by bit 11 (#ioapic_redir_t.destmod).
    union [[gnu::packed]] {
        uint64_t logdest : 8; //!< Logical destination in Logical Mode (r/w).
        struct [[gnu::packed]] {
            uint64_t apicid : 4; //!< APIC ID in Physical Mode (r/w).
            uint64_t reserved2 : 4;
        };
    };
} ioapic_redir_t;

/**
 * @{
 * @anchor DapicREDIR
 * @name APIC redirection entry field enums.
 */
/**
 * I/O APIC Redirection Entry Delivery Mode.
 * See #ioapic_redir_t.delmod.
 */
typedef enum {
    /**
     * Deliver on INTR of all destination processors.
     * Trigger Mode can be edge or level.
     */
    IOAPIC_DELMOD_FIXED = 0b000,
    /**
     * Deliver on INTR of the destination CPU running at lowest priority.
     * Trigger Mode can be edge or level.
     */
    IOAPIC_DELMOD_LOWPRI = 0b001,
    /**
     * System Management Interrupt.
     * Trigger Mode must be edge, Interrupt Vector must be all zeroes.
     */
    IOAPIC_DELMOD_SMI = 0b010,
    /**
     * Deliver on NMI of all destination processors.
     * Trigger Mode must be edge.
     */
    IOAPIC_DELMOD_NMI = 0b100,
    /**
     * Deliver on INIT of all destination processors.
     * Trigger Mode must be edge.
     */
    IOAPIC_DELMOD_INIT = 0b101,
    /**
     * Deliver on INTR of all destination processors as an interrupt that
     * originiated in an external controller.
     * Trigger Mode must be edge.
     */
    IOAPIC_DELMOD_EXTINT = 0b111,
} ioapic_delmod_t;

/**
 * I/O APIC Redirection Entry Destination Mode.
 * See #ioapic_redir_t.destmod.
 */
typedef enum {
    /**
     * Physical Mode.
     * Destination Field is interpreted as an APIC ID.
     */
    IOAPIC_DESTMOD_PHYSICAL = 0,
    /**
     * Logical Mode.
     * Destination Field is interpreted as a logical APIC ID.
     */
    IOAPIC_DESTMOD_LOGICAL = 1,
} ioapic_destmod_t;

/**
 * I/O APIC Redirection Entry Delivery Status.
 * See #ioapic_redir_t.delivs.
 */
typedef enum {
    /**
     * Idle.
     * There is currently no activity for this interrupt.
     */
    IOAPIC_DELIVS_IDLE,
    /**
     * Send pending.
     * The interrupt has been injected, but its delivery is held up due to the
     * bus being busy or the inability of the APIC to receive it.
     */
    IOAPIC_DELIVS_SEND_PENDING,
} ioapic_delivs_t;

/**
 * I/O APIC Redirection Entry Interrupt Polarity.
 * See #ioapic_redir_t.intpol.
 */
typedef enum {
    IOAPIC_INTPOL_ACTIVE_HIGH,
    IOAPIC_INTPOL_ACTIVE_LOW,
} ioapic_intpol_t;

/**
 * I/O APIC Redirection Entry Remote IRR (interrupt request register).
 * Meaningful only for level-triggered interrupts.
 * See #ioapic_redir_t.remirr.
 */
typedef enum {
    /// EOI message with a matching vector has been received from a local APIC.
    IOAPIC_REMIRR_EOI = 0,
    /// Local APIC(s) have accepted the interrupt.
    IOAPIC_REMIRR_ACK = 1,
} ioapic_remirr_t;

/**
 * I/O APIC Redirection Entry Trigger Mode.
 * See #ioapic_redir_t.trigmod.
 */
typedef enum {
    IOAPIC_TRIGMOD_EDGE = 0,
    IOAPIC_TRIGMOD_LEVEL = 1,
} ioapic_trigmod_t;
/**
 * @}
 */
