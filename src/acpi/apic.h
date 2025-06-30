/**
 * @file apic.h
 * I/O APIC and Local APIC driver API.
 */

#pragma once

#include <stdint.h>

#include "acpi/apic_regs.h"

/**
 * Initializes and enables the I/O APIC and the Local APIC.
 */
void apic_init(void);

/// Identity maps all the virtual pages of the APIC memory-mapped registers.
void apic_map_pages(void);

/**
 * Maps the external IRQ @a irq_num to the IDT vector @a vec_num.
 * @param irq_num IRQ number (e.g., 1 for keyboard).
 * @param vec_num Entry index in the IDT (see #gp_idt).
 * @returns
 * `true` if the remap was successful, otherwise `false`. A remap may fail,
 * e.g., due to the I/O APIC not being initialized.
 * @note
 * This function takes into account ACPI interrupt source overriding.
 */
bool apic_map_irq(uint8_t irq_num, uint8_t vec_num);

/**
 * Sets the I/O APIC Redirect Table Entry at index @a gsi to @a redir.
 * @param gsi   Global Source Interrupt number (Redirect Table index).
 * @param redir Redirect Table Entry values.
 * @returns
 * `true` if the entry for @a gsi has been set, otherwise `false`, e.g., if
 * there is no such GSI.
 */
bool apic_set_redirect(uint32_t gsi, const ioapic_redir_t *redir);

/**
 * Indicates to the Local APIC that the current interrupt is serviced.
 * @warning
 * This function does not check if the LAPIC register pointer is initialized.
 */
void apic_send_eoi(void);

/**
 * Clears the Local APIC Error Status register.
 * See #lapic_regs_t.esr.
 */
void lapic_clear_ers(void);

/**
 * Issues an Interrupt Command (an Inter-Processor Interrupt).
 * @param icr Interrupt Command Register value to copy to the ICR register.
 * See #lapic_icr_t, #lapic_regs_t.icr_63_32, #lapic_regs_t.icr_31_0.
 */
void lapic_send_ipi(const lapic_icr_t *icr);

/**
 * Waits for the IPI to be delivered.
 * See #lapic_icr_t.delivs.
 */
void lapic_wait_ipi_delivered(void);
