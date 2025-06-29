/**
 * @file apic.c
 * I/O APIC and Local APIC driver.
 */

#include "acpi/acpi.h"
#include "acpi/apic.h"
#include "acpi/apic_regs.h"
#include "cpu.h"
#include "kprintf.h"
#include "memfun.h"
#include "panic.h"
#include "pic.h"
#include "vmm.h"

static lapic_regs_t *g_lapic_regs;
static ioapic_regs_t *g_ioapic_regs;

static uint8_t g_ioapic_id;
static uint8_t g_ioapic_version;
static uint32_t g_ioapic_redirs;

static bool prv_lapic_init(void);
static bool prv_ioapic_init(void);
static void prv_ioapic_read_u32(ioapic_reg_sel_t regsel, void *buf);
static void prv_ioapic_read_u64(ioapic_reg_sel_t regsel, void *buf);
static void prv_ioapic_write_u32(ioapic_reg_sel_t regsel, const void *buf);
static void prv_ioapic_write_u64(ioapic_reg_sel_t regsel, const void *buf);

void apic_init(void) {
    pic_mask_all();

    if (!prv_lapic_init()) {
        kprintf("apic: failed to initialize Local APIC\n");
        panic("apic initialized failed");
    }

    if (!prv_ioapic_init()) {
        kprintf("apic: failed to initialize I/O APIC\n");
        panic("apic initialized failed");
    }
}

void apic_map_pages(void) {
    const uint32_t lapic_first_page = (uint32_t)g_lapic_regs;
    constexpr size_t lapic_num_pages = (sizeof(*g_lapic_regs) + 4095) / 4096;
    for (uint32_t idx = 0; idx < lapic_num_pages; idx++) {
        const uint32_t lapic_page = lapic_first_page + 4096 * idx;
        vmm_map_kernel_page(lapic_page, lapic_page);
    }

    const uint32_t ioapic_first_page = (uint32_t)g_ioapic_regs;
    constexpr size_t ioapic_num_pages = (sizeof(*g_ioapic_regs) + 4095) / 4096;
    for (uint32_t idx = 0; idx < ioapic_num_pages; idx++) {
        const uint32_t ioapic_page = ioapic_first_page + 4096 * idx;
        vmm_map_kernel_page(ioapic_page, ioapic_page);
    }
}

bool apic_map_irq(uint8_t irq_num, uint8_t vec_num) {
    if (!g_ioapic_regs) {
        kprintf("apic: cannot remap IRQ %u to vector %u: I/O APIC is not "
                "initialized\n",
                irq_num, vec_num);
        return false;
    }

    const acpi_irq_remap_t *const irq_remap = acpi_find_irq_remap(irq_num);
    const uint32_t gsi_num = irq_remap ? irq_remap->gsi : irq_num;

    const ioapic_redir_t redir = {
        .intvec = vec_num,
        .delmod = IOAPIC_DELMOD_FIXED,
        .destmod = IOAPIC_DESTMOD_PHYSICAL,
        .intpol = IOAPIC_INTPOL_ACTIVE_HIGH,
        .trigmod = IOAPIC_TRIGMOD_EDGE,
        .intmask = 0,
        .apicid = g_lapic_regs->lapic_id_bit.apic_id,
    };
    bool ok = apic_set_redirect(gsi_num, &redir);
    if (ok) { kprintf("apic: mapped IRQ %u to vector %u\n", irq_num, vec_num); }
    return ok;
}

bool apic_set_redirect(uint32_t gsi, const ioapic_redir_t *redir) {
    if (!g_ioapic_regs) {
        kprintf(
            "apic: cannot set I/O APIC GSI %u: I/O APIC is not initialized\n",
            gsi);
        return false;
    }
    if (gsi >= g_ioapic_redirs) {
        kprintf("apic: cannot set I/O APIC GSI %u: maximum GSI is %u\n", gsi,
                g_ioapic_redirs - 1);
        return false;
    }

    prv_ioapic_write_u64(IOAPIC_REG_REDIR(gsi), redir);

    // FIXME: read the value back and check if the r/w fields are the same as
    // in 'redir'?

    return true;
}

void apic_send_eoi(void) {
    g_lapic_regs->eoi = 0;
}

static bool prv_lapic_init(void) {
    // Set the global APIC enable bit in the IA32_APIC_BASE MSR.
    cpu_msr_apic_base_t msr_apic_base;
    msr_apic_base.val = cpu_read_msr(CPU_MSR_APIC_BASE);
    if (msr_apic_base.bit.apic_base >> 20) {
        kprintf("apic: MSR IA32_APIC_BASE address is beyond 4 GiB\n");
        return false;
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
    return true;
}

static bool prv_ioapic_init(void) {
    const acpi_ic_ioapic_t *const ics_ioapic = acpi_get_ioapic_ics();
    if (ics_ioapic) {
        g_ioapic_regs = (ioapic_regs_t *)ics_ioapic->ioapic_addr;
    } else {
        kprintf("apic: could not get I/O APIC ICS from acpi, using 0x%08X\n",
                acpiIOAPIC_FALLBACK_ADDR);
        g_ioapic_regs = (ioapic_regs_t *)acpiIOAPIC_FALLBACK_ADDR;
    }

    ioapic_reg_id_t reg_id;
    ioapic_reg_ver_t reg_ver;
    prv_ioapic_read_u32(IOAPIC_REG_ID, &reg_id);
    prv_ioapic_read_u32(IOAPIC_REG_VERSION, &reg_ver);

    g_ioapic_id = reg_id.ioapic_id;
    g_ioapic_version = reg_ver.version;
    g_ioapic_redirs = reg_ver.max_redir_entry + 1;

    kprintf("apic: I/O APIC 0x%02X version %u (%u entries) at %P\n",
            g_ioapic_id, g_ioapic_version, g_ioapic_redirs, g_ioapic_regs);
    return true;
}

[[maybe_unused]]
static void prv_ioapic_read_u32(ioapic_reg_sel_t regsel, void *buf) {
    g_ioapic_regs->regsel = regsel;
    uint32_t val = g_ioapic_regs->win;
    kmemcpy(buf, &val, 4);
}

[[maybe_unused]]
static void prv_ioapic_read_u64(ioapic_reg_sel_t regsel, void *buf) {
    uint32_t lo;
    uint32_t hi;

    g_ioapic_regs->regsel = regsel;
    lo = g_ioapic_regs->win;

    g_ioapic_regs->regsel = regsel + 1;
    hi = g_ioapic_regs->win;

    uint64_t val = ((uint64_t)hi << 32) | lo;
    kmemcpy(buf, &val, 8);
}

[[maybe_unused]]
static void prv_ioapic_write_u32(ioapic_reg_sel_t regsel, const void *buf) {
    uint32_t val;
    kmemcpy(&val, buf, 4);

    g_ioapic_regs->regsel = regsel;
    g_ioapic_regs->win = val;
}

[[maybe_unused]]
static void prv_ioapic_write_u64(ioapic_reg_sel_t regsel, const void *buf) {
    uint64_t val;
    kmemcpy(&val, buf, 8);
    const uint32_t lo = (uint32_t)val;
    const uint32_t hi = val >> 32;

    g_ioapic_regs->regsel = regsel;
    g_ioapic_regs->win = lo;

    g_ioapic_regs->regsel = regsel + 1;
    g_ioapic_regs->win = hi;
}
