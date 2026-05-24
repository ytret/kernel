#include "arch_timer.h"
#include "panic.h"

#include "arch/x86/apic/ioapic.h"
#include "arch/x86/apic/lapic.h"
#include "arch/x86/pit.h"

void arch_timer_init(void) {
    pit_init(PIT_PERIOD_MS);
    if (!ioapic_map_irq(PIT_IRQ, 32 + PIT_IRQ, lapic_get_id())) {
        PANIC("failed to map PIT IRQ");
    }
}

uint64_t arch_timer_current_ms(void) {
    return pit_counter_ms();
}

void arch_timer_busy_wait_ms(uint64_t msec) {
    pit_delay_ms(msec);
}
