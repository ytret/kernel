/**
 * @file lapic_regs.h
 * Local Advanced Programmable Interrupt Controller (LAPIC) register
 * definitions.
 *
 * Refer to Intel(R) 64 and IA-32 Architectures Software Developer's Manual,
 * Volume 3 (3A, 3B, 3C & 3D): System Programming Guide, December 2021 (later
 * revisions have different chapter numbers).
 */

#pragma once

#include "acpi/apic_common.h" // IWYU pragma: keep
#include "types.h"

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
 * Local APIC Interrupt Command Register structure.
 * Refer to section 10.6.1.
 */
typedef struct [[gnu::packed]] {
    uint64_t vector : 8;  //!< Interrupt vector number (r/w).
    uint64_t delmod : 3;  //!< Delivery Mode (r/w).
    uint64_t destmod : 1; //!< Destination Mode (r/w).
    uint64_t delivs : 1;  //!< Delivery Status (r/o).
    uint64_t reserved1 : 1;
    uint64_t level : 1;   //!< Level (r/w).
    uint64_t trigmod : 1; //!< Trigger Mode (r/w).
    uint64_t reserved2 : 2;
    uint64_t destsh : 2; //!< Destination Shorthand (r/w).
    uint64_t reserved3 : 12;

    uint64_t reserved4 : 24;
    uint64_t dest : 8; //!< Destination (r/w).
} lapic_icr_t;

/**
 * Local APIC Interrupt Command Register, field Delivery Mode.
 * See #lapic_icr_t.
 */
typedef enum {
    LAPIC_ICR_DELMOD_FIXED = 0b000,
    LAPIC_ICR_DELMOD_LOW_PRI = 0b001,
    LAPIC_ICR_DELMOD_SMI = 0b010,
    LAPIC_ICR_DELMOD_NMI = 0b100,
    LAPIC_ICR_DELMOD_INIT = 0b101,
    LAPIC_ICR_DELMOD_START_UP = 0b110,
} lapic_icr_delmod_t;

/**
 * Local APIC Interrupt Command Register, field Level.
 * See #lapic_icr_t.
 */
typedef enum {
    LAPIC_ICR_DEASSERT,
    LAPIC_ICR_ASSERT,
} lapic_icr_level_t;

/**
 * Local APIC Interrupt Command Register, field Destination Shorthand.
 * See #lapic_icr_t.
 */
typedef enum {
    LAPIC_ICR_DEST_NO_SHORTHAND = 0b00,
    LAPIC_ICR_DEST_SELF = 0b01,
    LAPIC_ICR_DEST_ALL_INC_SELF = 0b10,
    LAPIC_ICR_DEST_ALL_BUT_SELF = 0b11,
} lapic_icr_destsh_t;
