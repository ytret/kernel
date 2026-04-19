/**
 * @file apic_common.h
 * Common I/O and Local APIC enums.
 * See \l{lapic_regs.h} and \l{ioapic_regs.h}.
 */

#pragma once

/**
 * I/O APIC Redirection Entry Destination Mode, LAPIC ICR Destination Mode.
 * See #ioapic_redir_t.destmod, #lapic_icr_t.destmod.
 */
typedef enum {
    /**
     * Physical Mode.
     * Destination Field is interpreted as an APIC ID.
     */
    APIC_DESTMOD_PHYSICAL = 0,
    /**
     * Logical Mode.
     * Destination Field is interpreted as a logical APIC ID.
     */
    APIC_DESTMOD_LOGICAL = 1,
} apic_destmod_t;

/**
 * I/O APIC Redirection Entry Delivery Status, LAPIC ICR Delivery Status.
 * See #ioapic_redir_t.delivs, #lapic_icr_t.delivs.
 */
typedef enum {
    /**
     * Idle.
     * There is currently no activity for this interrupt.
     */
    APIC_DELIVS_IDLE,
    /**
     * Send pending.
     * The interrupt has been injected, but its delivery is held up due to the
     * bus being busy or the inability of the APIC to receive it.
     */
    APIC_DELIVS_SEND_PENDING,
} apic_delivs_t;

/**
 * I/O APIC Redirection Entry Trigger Mode, LAPIC ICR Trigger Mode.
 * See #ioapic_redir_t.trigmod, #lapic_icr_t.trigmod.
 */
typedef enum {
    APIC_TRIGMOD_EDGE = 0,
    APIC_TRIGMOD_LEVEL = 1,
} apic_trigmod_t;
