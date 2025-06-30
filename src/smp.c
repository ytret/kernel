#include <cpuid.h>
#include <stddef.h>
#include <stdint.h>

#include "acpi/acpi.h"
#include "acpi/apic.h"
#include "kprintf.h"
#include "memfun.h"
#include "panic.h"
#include "pit.h"
#include "smp.h"
#include "vmm.h"

#define SMP_AP_TRAMPOLINE_ADDR 0x8000

static uint8_t prv_smp_get_lapic_id(void);
static void prv_smp_init_trampoline(void);

// Defined in smp.s.
extern void smp_ap_trampoline(void);

void smp_init(void) {
    const uint8_t bsp_lapic = prv_smp_get_lapic_id();
    kprintf("smp: BSP's Local APIC ID = 0x%02X\n", bsp_lapic);

    prv_smp_init_trampoline();

    const uint8_t num_procs = acpi_num_procs();
    for (uint8_t proc_num = 0; proc_num < num_procs; proc_num++) {
        const acpi_proc_t *const proc = acpi_get_proc(proc_num);
        if (proc->lapic_id == bsp_lapic) { continue; }

        lapic_clear_ers();

        // Send INIT IPI.
        lapic_icr_t ipi_init = {
            .vector = 0,
            .delmod = LAPIC_ICR_DELMOD_INIT,
            .destmod = APIC_DESTMOD_PHYSICAL,
            .level = LAPIC_ICR_ASSERT,
            .trigmod = APIC_TRIGMOD_LEVEL,
            .destsh = LAPIC_ICR_DEST_NO_SHORTHAND,
            .dest = proc->lapic_id,
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
                .dest = proc->lapic_id,
            };
            lapic_send_ipi(&ipi_startup);
            pit_delay_ms(1);
            lapic_wait_ipi_delivered();
        }
    }

    for (;;) {
        __asm__ volatile("hlt");
    }
}

static uint8_t prv_smp_get_lapic_id(void) {
    unsigned int unused;
    unsigned int ebx;
    const int ok = __get_cpuid(1, &unused, &ebx, &unused, &unused);
    if (!ok) {
        panic_enter();
        kprintf("smp: failed to get CPUID leaf 1\n");
        panic("unexpected behavior");
    }
    return ebx >> 24;
}

static void prv_smp_init_trampoline(void) {
    // vmm_map_kernel_page(SMP_AP_TRAMPOLINE_ADDR, SMP_AP_TRAMPOLINE_ADDR);
    static_assert(SMP_AP_TRAMPOLINE_ADDR % 4096 == 0);

    kmemcpy((void *)SMP_AP_TRAMPOLINE_ADDR, smp_ap_trampoline, 4096);
}
