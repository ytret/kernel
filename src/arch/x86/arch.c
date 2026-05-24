#include "arch.h"
#include "arch_vmm.h"
#include "assert.h"
#include "init.h"
#include "kbd.h"
#include "kinttypes.h"
#include "log.h"
#include "panic.h"
#include "pit.h"
#include "smp.h"

#include "arch/x86/apic/ioapic.h"
#include "arch/x86/apic/lapic.h"
#include "arch/x86/arch_smp.h"
#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"

extern int stacktrace_walk(uint32_t *arr_addr, uint32_t max_items,
                           uint32_t init_ebp);

void arch_early_init(void) {
    gdtr_t gdtr;
    gdt_init_pre_smp(&gdtr);
    gdt_load(&gdtr);

    idt_init();
}

void arch_late_init(void) {
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

    __asm__ volatile("sti");
    LOG_DEBUG("interrupts enabled");

    lapic_calib_tim();
}

void arch_create_platform_tasks(void) {
    taskmgr_local_new_kernel_task("kbd", (uint32_t)kbd_task);
}

void arch_halt_until_int(void) {
    __asm__ volatile("hlt");
}

void arch_pause_in_loop(void) {
    __asm__ volatile("pause" ::: "memory");
}

void arch_disable_ints(void) {
    __asm__ volatile("cli");
}

void arch_enable_ints(void) {
    __asm__ volatile("sti");
}

void arch_get_ints_enabled(void) {
    PANIC("TODO");
}

void arch_ack_int(void) {
    lapic_send_eoi();
}

void arch_map_irq(uint32_t irq, uint32_t vec) {
    const uint8_t lapic_id = lapic_get_id();
    LOG_DEBUG("map irq %" PRIu32 " to vector %" PRIu32 " of LAPIC %u", irq, vec,
              lapic_id);
    ioapic_map_irq(irq, vec, lapic_id);
}

void arch_send_ipi(uint8_t proc_num, uint8_t vector) {
    smp_proc_t *const proc = smp_get_proc(proc_num);
    ASSERT(proc != NULL);

    arch_smp_proc_t *const arch_proc = proc->arch_ctx;
    ASSERT(arch_proc != NULL);
    ASSERT(arch_proc->acpi != NULL);

    const lapic_icr_t icr = {
        .vector = vector,
        .delmod = LAPIC_ICR_DELMOD_FIXED,
        .destmod = APIC_DESTMOD_PHYSICAL, // ignored because destsh is used
        .level = LAPIC_ICR_ASSERT, // must be ASSERT because it's not INIT
        .trigmod = 0,              // ignored because it's not INIT
        .destsh = LAPIC_ICR_DEST_NO_SHORTHAND,
        .dest = arch_proc->acpi->lapic_id,
    };
    lapic_send_ipi(&icr);
}

void arch_broadcast_ipi(uint8_t vector) {
    const lapic_icr_t icr = {
        .vector = vector,
        .delmod = LAPIC_ICR_DELMOD_FIXED,
        .destmod = APIC_DESTMOD_PHYSICAL, // ignored because destsh is used
        .level = LAPIC_ICR_ASSERT, // must be ASSERT because it's not INIT
        .trigmod = 0,              // ignored because it's not INIT
        .destsh = LAPIC_ICR_DEST_ALL_BUT_SELF,
        .dest = 0, // ignored because destsh is used
    };
    lapic_send_ipi(&icr);
}

void arch_ack_ipi(void) {
    lapic_send_eoi();
}

void arch_flush_tlb_addr(vaddr_t addr) {
    vmm_invlpg(addr);
}

void arch_flush_tlb(void) {
    PANIC("TODO");
}

[[gnu::noreturn]] void arch_init_bsp_task(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    arch_enable_ints();

    lapic_init_tim(LAPIC_TIM_PERIOD_MS);
    init_bsp_task_common();
}

[[gnu::noreturn]] void arch_init_ap_task(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    arch_enable_ints();

    lapic_init_tim(LAPIC_TIM_PERIOD_MS);
    init_ap_task_common();
}

size_t arch_walk_stack(vaddr_t *arr_addr, size_t max_items, vaddr_t init_sp) {
    static_assert(sizeof(vaddr_t) == sizeof(uint32_t));
    ASSERT(max_items <= UINT32_MAX);
    const uint32_t num_items = stacktrace_walk(
        (uint32_t *)arr_addr, (uint32_t)max_items, (uint32_t)init_sp);
    return (size_t)num_items;
}
