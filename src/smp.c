#include <cpuid.h>
#include <stddef.h>
#include <stdint.h>

#include "acpi/acpi.h"
#include "acpi/lapic.h"
#include "gdt.h"
#include "heap.h"
#include "idt.h"
#include "init.h"
#include "kprintf.h"
#include "memfun.h"
#include "panic.h"
#include "pit.h"
#include "smp.h"
#include "spinlock.h"
#include "vmm.h"

// physical/virtual (identity-mapped)
#define SMP_AP_TRAMPOLINE_ADDR 0x8000
#define SMP_AP_TRAMPLINE_ARGS  0x8800

// virtual
#define SMP_AP_INIT_STACK_TOP 0xA000 // page 0x9000..0x9FFF

typedef struct [[gnu::packed]] {
    // WARNING: the order, alignment and size of these fields is relied upon in
    // smp_ap_trampoline() in smp.s.
    uint8_t gdt_desc[6];
    uint32_t stack_top_virt;
    uint32_t pgdir_phys;
} smp_ap_trampoline_args_t;

typedef struct {
    uint32_t addr;
    _Atomic int ack_count;
} smp_tlb_shootdown_req_t;

static bool g_smp_is_active;

/**
 * Flag: BSP has finished initializing the APs and reached the initial task.
 */
static _Atomic bool g_smp_bsp_done;

/**
 * Flag: the AP currently being initialized has reached the initial task.
 */
static _Atomic bool g_smp_curr_ap_done;

static smp_proc_t *g_smp_procs;
static uint8_t g_smp_num_procs;

static spinlock_t g_smp_tlb_shootdown_lock;
static smp_tlb_shootdown_req_t g_smp_tlb_shootdown_req;

static void prv_smp_init_trampoline(const gdtr_t *gdtr);

// Defined in smp.s.
extern void smp_ap_trampoline(void);

void smp_init(void) {
    spinlock_init(&g_smp_tlb_shootdown_lock);

    const uint8_t num_procs = acpi_num_procs();
    const uint8_t bsp_lapic = lapic_get_id();
    kprintf("smp: BSP's Local APIC ID = 0x%02X\n", bsp_lapic);

    // Allocate the processor context array for the total number of processors.
    // Some of them may be unusable, but for simplicity we allocate for them,
    // too.
    g_smp_procs = heap_alloc(num_procs * sizeof(smp_proc_t));

    // If there are more than 1 usable processors, we consider the SMP to be
    // "active". The most important effect is that entering a kernel panic on
    // one of the processors causes a "halt" IPI to be sent by the panicking
    // processor. See panic.c.
    if (num_procs > 1) { g_smp_is_active = true; }

    for (uint8_t proc_num = 0; proc_num < num_procs; proc_num++) {
        const acpi_proc_t *const acpi_proc = acpi_get_proc(proc_num);

        // Create a kernel SMP context for each usable processor.
        smp_proc_t *const smp_proc = &g_smp_procs[g_smp_num_procs];
        if (acpi_proc->enabled) {

            gdt_init_for_proc(&smp_proc->gdt, &smp_proc->tss, &smp_proc->gdtr);

            smp_proc->proc_num = g_smp_num_procs;
            smp_proc->acpi = acpi_proc;

            g_smp_num_procs++;

            // FIXME: skip disabled processors.
        }

        if (acpi_proc->lapic_id == bsp_lapic) { continue; }

        g_smp_curr_ap_done = false;
        prv_smp_init_trampoline(&smp_proc->gdtr);

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

            static_assert(SMP_AP_TRAMPOLINE_ADDR == 0x8000,
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
        while (!g_smp_curr_ap_done) {
            __asm__ volatile("pause" ::: "memory");
        }
        kprintf("smp: AP UID %u (LAPIC ID %u) is up and running\n",
                acpi_proc->proc_uid, acpi_proc->lapic_id);
    }

    gdt_load(&smp_get_running_proc()->gdtr);
}

bool smp_is_active(void) {
    return g_smp_is_active;
}

bool smp_is_bsp_ready(void) {
    return g_smp_bsp_done;
}

void smp_set_bsp_ready(void) {
    g_smp_bsp_done = true;
}

void smp_set_ap_ready(void) {
    g_smp_curr_ap_done = true;
}

uint8_t smp_get_num_procs(void) {
    return g_smp_num_procs;
}

smp_proc_t *smp_get_proc(uint8_t proc_num) {
    if (g_smp_procs && proc_num < g_smp_num_procs) {
        return &g_smp_procs[proc_num];
    } else {
        return NULL;
    }
}

smp_proc_t *smp_get_running_proc(void) {
    if (g_smp_procs) {
        for (uint8_t proc_num = 0; proc_num < g_smp_num_procs; proc_num++) {
            smp_proc_t *const proc = &g_smp_procs[proc_num];
            if (proc->acpi->lapic_id == lapic_get_id()) { return proc; }
        }
    }
    return NULL;
}

taskmgr_t *smp_get_running_taskmgr(void) {
    smp_proc_t *proc = smp_get_running_proc();
    if (proc) {
        return proc->taskmgr;
    } else {
        return NULL;
    }
}

void smp_init_proc_taskmgr(void) {
    taskmgr_t *const taskmgr = heap_alloc(sizeof(*taskmgr));
    kmemset(taskmgr, 0, sizeof(*taskmgr));
}

void smp_send_tlb_shootdown(uint32_t addr) {
    spinlock_acquire(&g_smp_tlb_shootdown_lock);

    g_smp_tlb_shootdown_req.addr = addr;
    g_smp_tlb_shootdown_req.ack_count = 0;

    const lapic_icr_t ipi_tlb_shootdown = {
        .vector = SMP_VEC_TLB_SHOOTDOWN,
        .delmod = LAPIC_ICR_DELMOD_FIXED,
        .destmod = APIC_DESTMOD_PHYSICAL, // ignored because destsh is used
        .level = LAPIC_ICR_ASSERT, // must be ASSERT because it's not INIT
        .trigmod = 0,              // ignored because it's not INIT
        .destsh = LAPIC_ICR_DEST_ALL_BUT_SELF,
        .dest = 0, // ignored because destsh is used
    };
    lapic_send_ipi(&ipi_tlb_shootdown);

    const uint8_t num_procs = acpi_num_procs();
    while (g_smp_tlb_shootdown_req.ack_count < num_procs - 1) {
        __asm__ volatile("pause");
    }

    spinlock_release(&g_smp_tlb_shootdown_lock);
}

void smp_tlb_shootdown_handler(void) {
    vmm_invlpg(g_smp_tlb_shootdown_req.addr);
    g_smp_tlb_shootdown_req.ack_count++;
    lapic_send_eoi();
}

[[gnu::noreturn]]
void smp_ap_trampoline_c(void) {
    // NOTE: this function is called from 'smp_ap_trampoline' in smp.s.

    vmm_load_dir(vmm_kvas_dir());

    kprintf(
        "smp: Hello, World! from AP kernel num %u UID %u with LAPIC ID %u\n",
        smp_get_running_proc()->proc_num,
        smp_get_running_proc()->acpi->proc_uid,
        smp_get_running_proc()->acpi->lapic_id);
    idt_load(idt_get_desc());
    lapic_init(false);

    taskmgr_local_init(init_ap_task);

    panic_enter();
    panic("End of smp_ap_trampoline_c\n");
}

static void prv_smp_init_trampoline(const gdtr_t *gdtr) {
    // NOTE: it is assumed that everything below the kernel is identity mapped.

    static_assert(SMP_AP_TRAMPOLINE_ADDR % 4096 == 0);
    static_assert(SMP_AP_INIT_STACK_TOP % 4096 == 0);

    kmemcpy((void *)SMP_AP_TRAMPOLINE_ADDR, smp_ap_trampoline, 4096);
    kmemclr_sse2((void *)(SMP_AP_INIT_STACK_TOP - 4096), 4096);

    smp_ap_trampoline_args_t args = {
        .pgdir_phys = (uint32_t)vmm_kvas_dir(),
        .stack_top_virt = SMP_AP_INIT_STACK_TOP,
    };
    kmemcpy(&args.gdt_desc, gdtr, GDT_NUM_SMP_SEGS);
    kmemcpy((void *)SMP_AP_TRAMPLINE_ARGS, &args, sizeof(args));
}
