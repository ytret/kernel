/**
 * @file lapic.h
 * Local Advanced Programmable Interrupt Controller (LAPIC) driver API.
 */

#pragma once

#include "acpi/lapic_regs.h"

#define LAPIC_VEC_TIM       0xF0
#define LAPIC_TIM_PERIOD_MS 10

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

/**
 * Calibrates the Local APIC Timer counting frequency using the PIT.
 * See #pit_delay_ms(), #g_lapic_tim_freq_hz.
 * @note The interrupts must be enabled for #pit_delay_ms() to work.
 */
void lapic_calib_tim(void);

/**
 * Initializes the Local APIC Timer for the current processor.
 * @param period_ms Interrupt period (milliseconds).
 * See #lapic_tim_irq_handler().
 * @note The timer counting frequency must be already calibrated with
 * #lapic_calib_tim().
 */
void lapic_init_tim(uint32_t period_ms);

void lapic_tim_irq_handler(void);
