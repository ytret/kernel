#include "arch.h"
#include "kbd.h"
#include "log.h"
#include "panic.h"
#include "pit.h"
#include "vmm.h"

#include "arch/x86/apic/ioapic.h"
#include "arch/x86/apic/lapic.h"
#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"

void arch_init_1(void) {
    gdtr_t gdtr;
    gdt_init_pre_smp(&gdtr);
    gdt_load(&gdtr);

    idt_init();
}

void arch_init_2(void) {
    lapic_init(true);
    ioapic_init();

    pit_init(PIT_PERIOD_MS);
    if (!ioapic_map_irq(PIT_IRQ, 32 + PIT_IRQ, lapic_get_id())) {
        PANIC("failed to map PIT IRQ");
    }

    kbd_init();
    if (!ioapic_map_irq(KBD_IRQ, 32 + KBD_IRQ, lapic_get_id())) {
        PANIC("failed to map kbd IRQ");
    }

    vmm_init();

    lapic_map_pages();
    ioapic_map_pages();

    __asm__ volatile("sti");
    LOG_DEBUG("interrupts enabled");

    lapic_calib_tim();
}
