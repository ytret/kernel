#include "acpi/acpi.h"
#include "arch.h"
#include "arch_vmm.h"
#include "assert.h"
#include "heap.h"
#include "isrs.h"
#include "log.h"
#include "memfun.h"
#include "pmm.h"

#include "arch/x86/apic/lapic.h"
#include "arch/x86/arch_smp.h"
#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"
#include "arch/x86/pit.h"

typedef struct [[gnu::packed]] {
    // WARNING: the order, alignment and size of these fields is relied upon in
    // smp_ap_trampoline() in smp.s.
    uint8_t gdt_desc[6];
    uint32_t stack_top_virt;
    uint32_t pgdir_phys;
} smp_ap_trampoline_args_t;

static void prv_arch_smp_init_df_tss(smp_proc_t *proc);
static void prv_arch_smp_init_trampoline(const gdtr_t *gdtr);

// Defined in entry.s.
extern void entry_ap_trampoline(void);
extern void enable_sse(void);

size_t arch_smp_num_procs(void) {
    return (size_t)acpi_num_procs();
}

void arch_smp_init_proc_ctx(smp_proc_t *proc) {
    const uint8_t bsp_lapic = lapic_get_id();
    const acpi_proc_t *const acpi_proc = acpi_get_proc(proc->proc_num);

    arch_smp_proc_t *const arch_proc =
        heap_alloc_aligned(sizeof(arch_smp_proc_t), _Alignof(arch_smp_proc_t));
    kmemset(arch_proc, 0, sizeof(arch_smp_proc_t));

    proc->arch_ctx = arch_proc;

    // Create a kernel SMP context for each usable processor.
    if (acpi_proc->enabled) {
        gdt_init_for_proc(&arch_proc->gdt, &arch_proc->tss, &arch_proc->df_tss,
                          &arch_proc->gdtr);
        prv_arch_smp_init_df_tss(proc);

        arch_proc->acpi = acpi_proc;

        // FIXME: skip disabled processors.
    }

    if (acpi_proc->lapic_id == bsp_lapic) {
        proc->is_bsp = true;
    } else {
        proc->is_bsp = false;
    }

    if (proc->is_bsp) { return; }
}

void arch_smp_init_ap(smp_proc_t *proc) {
    ASSERT(!proc->is_bsp);

    const acpi_proc_t *const acpi_proc = acpi_get_proc(proc->proc_num);
    arch_smp_proc_t *const arch_proc = proc->arch_ctx;

    smp_reset_ap_ready();
    prv_arch_smp_init_trampoline(&arch_proc->gdtr);

    lapic_clear_ers();

    // Send INIT IPI.
    lapic_icr_t ipi_init = {
        .vector = 0,
        .delmod = LAPIC_ICR_DELMOD_INIT,
        .destmod = APIC_DESTMOD_PHYSICAL,
        .level = LAPIC_ICR_ASSERT,
        .trigmod = APIC_TRIGMOD_LEVEL,
        .destsh = LAPIC_ICR_DEST_NO_SHORTHAND,
        .dest = acpi_proc->lapic_id,
    };
    lapic_send_ipi(&ipi_init);
    lapic_wait_ipi_delivered();
    ipi_init.level = LAPIC_ICR_DEASSERT;
    lapic_send_ipi(&ipi_init);
    lapic_wait_ipi_delivered();

    pit_delay_ms(10);

    // Send START UP IPI twice.
    for (int i = 0; i < 2; i++) {
        lapic_clear_ers();

        static_assert(ARCH_AP_TRAMPOLINE_ADDR == 0x8000,
                      "please update ipi_startup.vector");
        const lapic_icr_t ipi_startup = {
            // Vector number V -> address 0x000VV000.
            // Refer to section 8.4.3.
            .vector = 8,
            .delmod = LAPIC_ICR_DELMOD_START_UP,
            .destmod = APIC_DESTMOD_PHYSICAL,
            .level = LAPIC_ICR_DEASSERT,
            .trigmod = APIC_TRIGMOD_EDGE,
            .destsh = LAPIC_ICR_DEST_NO_SHORTHAND,
            .dest = acpi_proc->lapic_id,
        };
        lapic_send_ipi(&ipi_startup);
        pit_delay_ms(1);
        lapic_wait_ipi_delivered();
    }

    // Wait for the current AP to initialize.
    while (!smp_is_ap_ready()) {
        __asm__ volatile("pause" ::: "memory");
    }
    LOG_INFO("AP UID %u (LAPIC ID %u) is up and running", acpi_proc->proc_uid,
             acpi_proc->lapic_id);
}

void arch_smp_init_bsp(void) {
    const uint8_t bsp_lapic = lapic_get_id();
    LOG_DEBUG("BSP's Local APIC ID = 0x%02X", bsp_lapic);

    smp_proc_t *const running_proc = smp_get_running_proc();
    ASSERT(running_proc != NULL);
    ASSERT(running_proc->is_bsp);

    arch_smp_proc_t *const arch_proc = running_proc->arch_ctx;

    gdt_load(&arch_proc->gdtr);
}

smp_proc_t *arch_smp_get_running_proc(void) {
    const uint8_t num_procs = smp_get_num_procs();
    for (uint8_t proc_num = 0; proc_num < num_procs; proc_num++) {
        smp_proc_t *const proc = smp_get_proc(proc_num);
        DEBUG_ASSERT(proc != NULL);

        const arch_smp_proc_t *const arch_proc = proc->arch_ctx;
        if (arch_proc->acpi->lapic_id == lapic_get_id()) { return proc; }
    }

    return NULL;
}

[[gnu::noreturn]]
void arch_smp_ap_trampoline_c(void) {
    // NOTE: this function is called from 'smp_ap_trampoline' in smp.s.

    enable_sse();

    smp_proc_t *const proc = smp_get_running_proc();
    ASSERT(proc != NULL);

    arch_smp_proc_t *const arch_proc = proc->arch_ctx;
    ASSERT(arch_proc != NULL);

    LOG_DEBUG("Hello, World! from AP kernel num %u UID %u with LAPIC ID %u",
              proc->proc_num, arch_proc->acpi->proc_uid,
              arch_proc->acpi->lapic_id);
    idt_load(idt_get_desc());
    lapic_init(false);

    smp_ap_trampoline_c();
}

static void prv_arch_smp_init_df_tss(smp_proc_t *proc) {
    static_assert(SMP_DF_STACK_SIZE % PMM_PAGE_SIZE == 0);

    if (!proc) { PANIC("invalid argument 'proc' value NULL"); }

    arch_smp_proc_t *const arch_proc = proc->arch_ctx;
    ASSERT(arch_proc != NULL);

    arch_proc->df_stack_bottom =
        heap_alloc_aligned(SMP_DF_STACK_SIZE, PMM_PAGE_SIZE);
    arch_proc->df_stack_top =
        (void *)((uintptr_t)arch_proc->df_stack_bottom + SMP_DF_STACK_SIZE);

    arch_proc->df_tss->esp0 = (uint32_t)arch_proc->df_stack_top;
    arch_proc->df_tss->esp = (uint32_t)arch_proc->df_stack_top;
    arch_proc->df_tss->ss0 = GDT_KERNEL_DATA_IDX << 3;
    arch_proc->df_tss->ss = GDT_KERNEL_DATA_IDX << 3;
    arch_proc->df_tss->ds = GDT_KERNEL_DATA_IDX << 3;
    arch_proc->df_tss->es = GDT_KERNEL_DATA_IDX << 3;
    arch_proc->df_tss->fs = GDT_KERNEL_DATA_IDX << 3;
    arch_proc->df_tss->gs = GDT_KERNEL_DATA_IDX << 3;
    arch_proc->df_tss->cs = GDT_KERNEL_CODE_IDX << 3;
    arch_proc->df_tss->eip = (uint32_t)isr_8;
    arch_proc->df_tss->cr3 = (uint32_t)vmm_kvas_dir();
}

static void prv_arch_smp_init_trampoline(const gdtr_t *gdtr) {
    // NOTE: it is assumed that everything below the kernel is identity mapped.

    static_assert(ARCH_AP_TRAMPOLINE_ADDR % 4096 == 0);
    static_assert(ARCH_AP_INIT_STACK_TOP % 4096 == 0);

    kmemcpy((void *)ARCH_AP_TRAMPOLINE_ADDR, entry_ap_trampoline, 4096);
    kmemclr_sse2((void *)(ARCH_AP_INIT_STACK_TOP - 4096), 4096);

    smp_ap_trampoline_args_t args = {
        .pgdir_phys = (uint32_t)vmm_kvas_dir(),
        .stack_top_virt = ARCH_AP_INIT_STACK_TOP,
    };
    kmemcpy(&args.gdt_desc, gdtr, GDT_NUM_SMP_SEGS);
    kmemcpy((void *)ARCH_AP_TRAMPLINE_ARGS, &args, sizeof(args));
}
