/**
 * @file ioapic.h
 * Input/Output Advanced Programmable Interrupt Controller (I/O APIC, IOAPIC)
 * driver API.
 */

#pragma once

#include <stdint.h>

#include "acpi/ioapic_regs.h"

/**
 * Initializes and enables the I/O APIC.
 * @note Masks all the PIC interrupts.
 */
void ioapic_init(void);

/// Identity maps the I/O memory-mapped register pages.
void ioapic_map_pages(void);

/**
 * Maps an external IRQ to a vector of the target LAPIC.
 *
 * @param irq_num  IRQ number (e.g., 1 for keyboard).
 * @param vec_num  Entry index in the IDT (see #gp_idt).
 * @param lapic_id Target Local APIC ID.
 *
 * @returns
 * `true` if the remap was successful, otherwise `false`. A remap may fail,
 * e.g., due to the I/O APIC not being initialized.
 *
 * @note
 * This function takes into account ACPI interrupt source overriding.
 */
bool ioapic_map_irq(uint8_t irq_num, uint8_t vec_num, uint8_t lapic_id);

/**
 * Sets the I/O APIC Redirect Table Entry at index @a gsi to @a redir.
 *
 * @param gsi   Global Source Interrupt number (Redirect Table index).
 * @param redir Redirect Table Entry values.
 *
 * @returns
 * `true` if the entry for @a gsi has been set, otherwise `false`, e.g., if
 * there is no such GSI.
 */
bool ioapic_set_redirect(uint32_t gsi, const ioapic_redir_t *redir);
