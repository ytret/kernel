#include "arch.h"
#include "arch_timer.h"
#include "arch_vmm.h"
#include "assert.h"
#include "framebuf.h"
#include "init.h"
#include "kbd.h"
#include "kinttypes.h"
#include "ksyscall.h"
#include "log.h"
#include "panic.h"
#include "smp.h"

#include "arch/x86/apic/ioapic.h"
#include "arch/x86/apic/lapic.h"
#include "arch/x86/arch_smp.h"
#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"
#include "arch/x86/mbi.h"
#include "arch/x86/vga.h"

// See arch/x86/linker.ld.
extern uint32_t ld_vmm_kernel_end;

void arch_entrypoint(uint32_t magic_num, uint32_t mbi_addr);
extern int stacktrace_walk(uint32_t *arr_addr, uint32_t max_items,
                           uint32_t init_ebp);
extern void main(void);

void arch_entrypoint(uint32_t magic_num, uint32_t mbi_addr) {
    mbi_init(magic_num, mbi_addr);
    main();
}

void arch_early_init(pmm_mmap_t *mmap) {
    mbi_check_magic();

    gdtr_t gdtr;
    gdt_init_pre_smp(&gdtr);
    gdt_load(&gdtr);

    idt_init();

    if (!mbi_fill_mmap(mbi_ptr(), mmap)) {
        PANIC("failed to fill the memory map");
    }
}

void arch_early_init_heap(void) {
    mbi_save_on_heap();
}

void arch_late_init(void) {
    lapic_init(true);
    ioapic_init();

    arch_timer_init();

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

const char *arch_get_cmdline(void) {
    if (mbi_ptr()->flags & MBI_FLAG_CMDLINE) {
        return (const char *)mbi_ptr()->cmdline;
    } else {
        return NULL;
    }
}

paddr_t arch_get_kernel_phys_end(void) {
    const mbi_mod_t *const last_mod = mbi_last_mod();
    const paddr_t kernel_end = VIRT_TO_PHYS(&ld_vmm_kernel_end);

    paddr_t last_used_addr;
    if (last_mod) { last_used_addr = last_mod->mod_end; }
    if (!last_mod || kernel_end > last_used_addr) {
        last_used_addr = kernel_end;
    }

    return (last_used_addr + 0x3FFFFFULL) & ~(0X3FFFFFULL);
}

void arch_textdisp_early_init(textdisp_t *disp) {
    mbi_t const *p_mbi = mbi_ptr();

    if ((p_mbi->flags & MBI_FLAG_FRAMEBUF) &&
        (MBI_FRAMEBUF_EGA != p_mbi->framebuffer_type)) {
        LOG_DEBUG("terminal type - framebuffer");

        framebuf_early_init();
        disp->max_row = framebuf_height_chars();
        disp->max_col = framebuf_width_chars();
        disp->ops = framebuf_textdisp_ops();
    } else {
        LOG_DEBUG("terminal type - VGA text mode");

        vga_early_init();
        disp->max_row = vga_height_chars();
        disp->max_col = vga_width_chars();
        disp->ops = vga_textdisp_ops();
    }

    disp->has_impl = true;
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
