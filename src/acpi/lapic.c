/**
 * @file lapic.h
 * Local Advanced Programmable Interrupt Controller (LAPIC) driver.
 */

#include <cpuid.h>

#include "acpi/lapic.h"
#include "cpu.h"
#include "kprintf.h"
#include "memfun.h"
#include "panic.h"
#include "vmm.h"

static lapic_regs_t *g_lapic_regs;

void lapic_init(void) {
    // Set the global APIC enable bit in the IA32_APIC_BASE MSR.
    cpu_msr_apic_base_t msr_apic_base;
    msr_apic_base.val = cpu_read_msr(CPU_MSR_APIC_BASE);
    if (msr_apic_base.bit.apic_base >> 20) {
        panic_enter();
        kprintf("apic: MSR IA32_APIC_BASE address is beyond 4 GiB\n");
        panic("unexpected behavior");
    }
    msr_apic_base.bit.apic_gl_en = 1;
    cpu_write_msr(CPU_MSR_APIC_BASE, msr_apic_base.val);
    g_lapic_regs =
        (lapic_regs_t *)((uint32_t)msr_apic_base.bit.apic_base << 12);

    // Mask LINT0 and LINT1.
    g_lapic_regs->lvt_lint0 = 1 << 16;
    g_lapic_regs->lvt_lint1 = 1 << 16;

    // Set the spurious IRQ to 0xFF (the lowest 4 bits must be set, see
    // section 10.9) and enable the LAPIC.
    g_lapic_regs->sivr_bit.sv = 0xFF; // other values won't work?
    g_lapic_regs->sivr_bit.ase = 1;

    kprintf("apic: Local APIC 0x%02X version %u (%u entries) at %P\n",
            g_lapic_regs->lapic_id_bit.apic_id,
            g_lapic_regs->lapic_version_bit.version,
            g_lapic_regs->lapic_version_bit.max_lvt_entry + 1, g_lapic_regs);
}

void lapic_map_pages(void) {
    const uint32_t lapic_first_page = (uint32_t)g_lapic_regs;
    constexpr size_t lapic_num_pages = (sizeof(*g_lapic_regs) + 4095) / 4096;
    for (uint32_t idx = 0; idx < lapic_num_pages; idx++) {
        const uint32_t lapic_page = lapic_first_page + 4096 * idx;
        vmm_map_kernel_page(lapic_page, lapic_page);
    }
}

uint8_t lapic_get_id(void) {
    unsigned int unused;
    unsigned int ebx;
    const int ok = __get_cpuid(1, &unused, &ebx, &unused, &unused);
    if (!ok) {
        panic_enter();
        kprintf("lapic: failed to get CPUID leaf 1\n");
        panic("unexpected behavior");
    }
    return ebx >> 24;
}

void lapic_clear_ers(void) {
    if (!g_lapic_regs) { panic("LAPIC register pointer is uninitialized"); }
    g_lapic_regs->esr = 0;
}

void lapic_send_ipi(const lapic_icr_t *icr) {
    if (!g_lapic_regs) { panic("LAPIC register pointer is uninitialized"); }

    uint64_t icr_buf;
    kmemcpy(&icr_buf, icr, sizeof(*icr));

    // Section 10.6.1 says that writing to the low dword of the ICR causes the
    // IPI to be sent.
    g_lapic_regs->icr_63_32 = icr_buf >> 32;
    g_lapic_regs->icr_31_0 = (uint32_t)icr_buf;
}

void lapic_wait_ipi_delivered(void) {
    const volatile lapic_icr_t *icr =
        (const volatile lapic_icr_t *)(&g_lapic_regs->icr_63_32);
    do {
        __asm__ volatile("pause" ::: "memory");
    } while (icr->delivs == APIC_DELIVS_SEND_PENDING);
}

void lapic_send_eoi(void) {
    g_lapic_regs->eoi = 0;
}
