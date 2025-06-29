/**
 * @file acpi_defs.h
 * ACPI structure definitions.
 * Refer to "Advanced Configuration and Power Interface Specification",
 * version 6.1.
 */

#pragma once

#include <stdint.h>

/**
 * Fallback I/O APIC Register interface address.
 * It can be used when there is no I/O APIC Interrupt Controller Structure
 * present in the MADT table (see #acpi_madt_t.ics).
 */
#define acpiIOAPIC_FALLBACK_ADDR 0xFEC00000

/**
 * Root System Description Pointer (RSDP) for ACPI 1.0.
 * Refer to section 5.2.5.3.
 */
typedef struct [[gnu::packed]] {
    uint8_t signature[8];
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t revision;
    uint32_t rsdt_addr;
} acpi_rsdp1_t;

/**
 * Root System Description Pointer (RSDP) for ACPI 2.0.
 * See #acpi_rsdp1_t.
 */
typedef struct [[gnu::packed]] {
    uint8_t signature[8];
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t revision;
    uint32_t rsdt_addr;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t ext_checksum;
    uint8_t reserved1[3];
} acpi_rsdp2_t;

/**
 * System Description Table Header.
 * Refer to section 5.2.6.
 */
typedef struct [[gnu::packed]] {
    uint8_t signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t oem_table_id[8];
    uint32_t oem_revision;
    uint8_t creator_id[4];
    uint32_t creator_revision;
} acpi_sdt_hdr_t;

/**
 * Root System Description Table (RSDT).
 * Refer to section 5.2.7.
 */
typedef struct [[gnu::packed]] {
    acpi_sdt_hdr_t header;
    uint32_t entries[];
} acpi_rsdt_t;

/**
 * Extended System Descriptor Table (XSDT).
 * Refer to section 5.2.8.
 */
typedef struct [[gnu::packed]] {
    acpi_sdt_hdr_t header;
    uint64_t entries[];
} acpi_xsdt_t;

/**
 * Mutliple APIC Description Table (MADT).
 * Refer to section 5.2.12.
 */
typedef struct [[gnu::packed]] {
    acpi_sdt_hdr_t header;
    uint32_t lapic_addr; //!< Local Interrupt Controller Address.

    /// Multiple APIC flags.
    union {
        uint32_t flags;
        struct {
            /**
             * System has (1) or does not have (0) a dual-8259 setup.
             * The 8259 vectors must be masked when enabling the APIC
             * operation.
             */
            uint32_t pcat_compat : 1;
            uint32_t reserved1 : 31;
        } flags_bit;
    };

    uint8_t ics[]; //!< Interrupt Controller Structures.
} acpi_madt_t;

/**
 * Interrupt Controller Structure Types in the MADT table.
 * See #acpi_madt_t.ics.
 * @note
 * Not all values that may be present in an actual MADT are present in this
 * enum.
 */
typedef enum {
    ACPI_MADT_ICS_LAPIC = 0x00,
    ACPI_MADT_ICS_IOAPIC = 0x01,
    ACPI_MADT_ICS_INT_SRC_OVR = 0x02,
    ACPI_MADT_ICS_NMI_SRC = 0x03,
    ACPI_MADT_ICS_LAPIC_NMI = 0x04,
    ACPI_MADT_ICS_LAPIC_ADDR_OVR = 0x05,
} acpi_madt_ics_t;

/**
 * Interrupt Controller Structure Type: Processor Local APIC.
 * Refer to section 5.2.12.2.
 * See #acpi_madt_t.ics.
 */
typedef struct [[gnu::packed]] {
    uint8_t type;     //!< Must be `0x00`.
    uint8_t length;   //!< Must be `8`.
    uint8_t proc_uid; //!< ACPI Processor UID.
    uint8_t lapic_id; //!< Processor's Local API ID.

    union {
        uint32_t flags;
        struct {
            /// Processor unusable (value `0`) flag.
            uint32_t enabled : 1;
            uint32_t reserved1 : 31;
        } flags_bit;
    };
} acpi_ic_lapic_t;

/**
 * Interrupt Controller Structure Type: I/O APIC.
 * Refer to section 5.2.12.3.
 * See #acpi_madt_t.ics.
 */
typedef struct [[gnu::packed]] {
    uint8_t type;      //!< Must be `0x01`.
    uint8_t length;    //!< Must be `12`.
    uint8_t ioapic_id; //!< I/O APIC's ID.
    uint8_t reserved1;
    uint32_t ioapic_addr;
    uint32_t gsi_base; //!< Global System Interrupt Base.
} acpi_ic_ioapic_t;

/**
 * MPS INTI flags, used in multiple Interrupt Controller Structure Types.
 * Refer to table 5-51.
 */
typedef union [[gnu::packed]] {
    uint16_t val;
    struct [[gnu::packed]] {
        uint16_t polarity : 2;
        uint16_t trigger_mode : 2;
        uint16_t reserved : 12;
    } bit;
} acpi_inti_flags;

/**
 * Interrupt Controller Structure Type: Interrupt Source Override.
 * Refer to section 5.2.12.5.
 * See #acpi_madt_t.ics.
 */
typedef struct [[gnu::packed]] {
    uint8_t type;   //!< Must be `0x02`.
    uint8_t length; //!< Must be `10`.
    uint8_t bus;    //!< Must be `0`, meaning ISA.
    uint8_t source; //!< Bus-relative interrupt source (IRQ).
    uint32_t gsi;   //!< Global System Interrupt that this source will signal.
    acpi_inti_flags flags;
} acpi_ic_int_src_ovr_t;

/**
 * Interrupt Controller Structure Type: Non-Maskable Interrupt (NMI) Source.
 * Refer to section 5.2.12.6.
 * See #acpi_madt_t.ics.
 */
typedef struct [[gnu::packed]] {
    uint8_t type;   //!< Must be `0x03`.
    uint8_t length; //!< Must be `8`.
    acpi_inti_flags flags;
    uint32_t gsi; //!< Global System Interrupt that this NMI will signal.
} acpi_ic_nmi_src_t;

/**
 * Interrupt Controller Structure Type: Local APIC NMI.
 * Refer to section 5.2.12.7.
 * See #acpi_madt_t.ics.
 */
typedef struct [[gnu::packed]] {
    uint8_t type;   //!< Must be `0x04`.
    uint8_t length; //!< Must be `6`.
    uint8_t proc_uid;
    acpi_inti_flags flags;
    uint8_t lapic_int_num; //!< LAPIC input LINTn to which NMI is connected.
} apic_ic_lapic_nmi_t;

/**
 * Interrupt Controller Structure Type: Local APIC Address Override.
 * Refer to section 5.2.12.8
 * See #acpi_madt_t.ics.
 */
typedef struct [[gnu::packed]] {
    uint8_t type;   //!< Must be `0x05`.
    uint8_t length; //!< Must be `12`.
    uint16_t reserved1;
    uint64_t lapic_addr; //!< Physical address of the LAPIC.
} apic_ic_lapic_addr_ovr_t;
