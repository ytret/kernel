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
#include "pit.h"
#include "taskmgr.h"
#include "vmm.h"

static lapic_regs_t *g_lapic_regs;
static uint32_t g_lapic_tim_freq_hz;

void lapic_init(bool is_bsp) {
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

    if (is_bsp) {
        g_lapic_regs =
            (lapic_regs_t *)((uint32_t)msr_apic_base.bit.apic_base << 12);
    }

    kprintf("apic: Local APIC 0x%02X version %u (%u entries) at %P\n",
            g_lapic_regs->lapic_id_bit.apic_id,
            g_lapic_regs->lapic_version_bit.version,
            g_lapic_regs->lapic_version_bit.max_lvt_entry + 1, g_lapic_regs);

    // Mask LINT0 and LINT1.
    g_lapic_regs->lvt_lint0 |= 1 << 16;
    g_lapic_regs->lvt_lint1 |= 1 << 16;

    // Set the spurious IRQ to 0xFF (the lowest 4 bits must be set, see
    // section 10.9) and enable the LAPIC.
    g_lapic_regs->sivr_bit.sv = 0xFF; // other values won't work?
    g_lapic_regs->sivr_bit.ase = 1;
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

void lapic_calib_tim(void) {
    constexpr uint32_t calib_dur_ms = 100;
    kprintf("lapic: calibrating Local APIC Timer for %u ms\n", calib_dur_ms);

    const lapic_lvt_tim_t lvt_tim = {
        .vector = LAPIC_VEC_TIM,
        .mask = 1,
        .tim_mode = LAPIC_TIM_MODE_PERIODIC,
    };
    uint32_t lvt_tim_val;
    kmemcpy(&lvt_tim_val, &lvt_tim, 4);
    g_lapic_regs->lvt_tim = lvt_tim_val;

    g_lapic_regs->dcr = LAPIC_DCR_DIV_8;
    g_lapic_regs->icr = 0xFFFFFFFF;

    pit_delay_ms(calib_dur_ms);

    const uint32_t cnt_diff = 0xFFFFFFFF - g_lapic_regs->ccr;
    const uint32_t freq_hz = 1000 / calib_dur_ms * cnt_diff;
    kprintf("lapic: timer frequency is %u Hz\n", freq_hz);

    g_lapic_tim_freq_hz = freq_hz;
}

void lapic_init_tim(uint32_t period_ms) {
    if (g_lapic_tim_freq_hz == 0) {
        panic_enter();
        kprintf("lapic: cannot initialize LAPIC Timer: not calibrated\n");
        panic("unexpected behavior");
    }

    const uint32_t init_cnt_val = g_lapic_tim_freq_hz * period_ms / 1000;
    kprintf("lapic: LAPIC %u: timer initial count %u, reload frequency %u Hz\n",
            lapic_get_id(), init_cnt_val, g_lapic_tim_freq_hz / init_cnt_val);
    g_lapic_regs->icr = init_cnt_val;

    const lapic_lvt_tim_t lvt_tim = {
        .vector = LAPIC_VEC_TIM,
        .mask = 0,
        .tim_mode = LAPIC_TIM_MODE_PERIODIC,
    };
    uint32_t lvt_tim_val;
    kmemcpy(&lvt_tim_val, &lvt_tim, 4);
    g_lapic_regs->lvt_tim = lvt_tim_val;
}

void lapic_tim_irq_handler(void) {
    lapic_send_eoi();
    taskmgr_local_schedule();
}
