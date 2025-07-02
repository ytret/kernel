/**
 * @file ioapic_regs.h
 * Input/Output Advanced Programmable Interrupt Controller (I/O APIC, IOAPIC)
 * register definitions.
 *
 * Refer to 82093AA I/O Advanced Programmable Interrupt Controller (IOAPIC)
 * datasheet, May 1996.
 */

#pragma once

#include "acpi/apic_common.h" // IWYU pragma: keep
#include "types.h"

/**
 * I/O APIC first Redirection Table Entry register number.
 * See #ioapic_regs_t and #ioapic_redir_t.
 */
#define IOAPIC_REG_REDIR(x) (0x10 + 2 * (x))

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
    IOAPIC_REG_ARBITR = 0x02,  //!< IOAPIC Arbitration Register.
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
 * I/O APIC Redirection Entry Delivery Mode.
 * Do not confuse with LAPIC Interrupt Delivery Mode (#lapic_icr_t.delmod).
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
