/**
 * @file lapic.h
 * Local Advanced Programmable Interrupt Controller (LAPIC) driver API.
 */

#pragma once

#include "acpi/lapic_regs.h"

void lapic_init(bool is_bsp);

/**
 * Identity maps the LAPIC memory-mapped register pages.
 * @note
 * If the kernel virtual address space is shared between the processors, this
 * needs to be done only once.
 */
void lapic_map_pages(void);

/// Returns the Local APIC ID of the running processor.
uint8_t lapic_get_id(void);

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

/**
 * Indicates to the Local APIC that the current interrupt is serviced.
 * @warning
 * This function does not check if the LAPIC register pointer is initialized.
 */
void lapic_send_eoi(void);
