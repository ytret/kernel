#pragma once

#include <stdint.h>

#include "acpi_defs.h"

typedef struct {
    uint8_t irq;  //!< Legacy IRQ number.
    uint32_t gsi; //!< Global System Interrupt (APIC redirection table index).
} acpi_irq_remap_t;

void acpi_init(void);

/**
 * Copies the local APIC registers base address to @a *out_addr.
 * @param[out] out_addr Pointer to the variable that will store the address.
 * @returns
 * `true` if MADT has been found previously during #acpi_init() and the LAPIC
 * address has been copied to @a *out_addr, otherwise `false`.
 */
bool acpi_get_lapic_base(uint32_t *out_addr);

/**
 * Returns the I/O APIC structure address on the heap.
 * @returns
 * - An address to the structure copied from ACPI. This is achieved only if:
 *   1. An MADT table has been found previously during #acpi_init().
 *   2. It contains an I/O APIC interrupt controller structure.
 * - `NULL` if no I/O APIC structure has been found.
 */
const acpi_ic_ioapic_t *acpi_get_ioapic_ics(void);

/**
 * Finds the override GSI of an IRQ @a irq.
 * @param irq Legacy IRQ number.
 * @returns IRQ remap information, if there is any, otherwise `NULL`.
 */
const acpi_irq_remap_t *acpi_find_irq_remap(uint8_t irq);
