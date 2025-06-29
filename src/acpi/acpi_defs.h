/**
 * @file acpi_defs.h
 * ACPI structure definitions.
 * Refer to "Advanced Configuration and Power Interface Specification",
 * version 6.1.
 */

#pragma once

#include <stdint.h>

/**
 * Root System Description Pointer (RSDP) for ACPI 1.0.
 * Refer to section 5.2.5.3.
 */
typedef struct [[gnu::packed]] {
    uint8_t signature[8];
    uint8_t checksum;
    uint8_t oemid[6];
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
    uint8_t oemid[6];
    uint8_t revision;
    uint32_t rsdt_addr;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} acpi_rsdp2_t;

/**
 * Root System Description Table (RSDT).
 * Refer to section 5.2.7.
 */
typedef struct [[gnu::packed]] {
    uint8_t signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t oem_table_id[8];
    uint8_t oem_revision[4];
    uint8_t creator_id[4];
    uint8_t creator_revision[4];
    uint32_t entries[];
} acpi_rsdt_t;

/**
 * Extended System Descriptor Table (XSDT).
 * Refer to section 5.2.8.
 */
typedef struct [[gnu::packed]] {
    uint8_t signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t oem_table_id[8];
    uint8_t oem_revision[4];
    uint8_t creator_id[4];
    uint8_t creator_revision[4];
    uint64_t entries[];
} acpi_xsdt_t;
