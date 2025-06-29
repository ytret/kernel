#include "acpi/acpi.h"
#include "acpi/apic.h"
#include "acpi/apic_regs.h"
#include "cpu.h"
#include "kprintf.h"
#include "memfun.h"
#include "panic.h"
#include "pic.h"

static lapic_regs_t *g_lapic_regs;
static ioapic_regs_t *g_ioapic_regs;

static uint8_t g_ioapic_id;
static uint8_t g_ioapic_version;
static uint32_t g_ioapic_redirs;

static bool prv_lapic_init(void);

static bool prv_ioapic_init(void);
static uint32_t prv_ioapic_read_u32(ioapic_reg_sel_t regsel);
static void prv_ioapic_write_u32(ioapic_reg_sel_t regsel, uint32_t val);
static void prv_ioapic_write_u64(ioapic_reg_sel_t regsel, uint64_t val);

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

static bool prv_lapic_init(void) {
    // Set the global APIC enable bit in the IA32_APIC_BASE MSR.
    cpu_msr_apic_base_t msr_apic_base;
    msr_apic_base.val = cpu_read_msr(CPU_MSR_APIC_BASE);
    if (msr_apic_base.bit.apic_base >> 32) {
        kprintf("apic: MSR IA32_APIC_BASE address is beyond 4 GiB\n");
        return false;
    }
    msr_apic_base.bit.apic_gl_en = 1;
    cpu_write_msr(CPU_MSR_APIC_BASE, msr_apic_base.val);
    g_lapic_regs = (lapic_regs_t *)(uint32_t)msr_apic_base.bit.apic_base;

    // Set the spurious IRQ to 47 (the lowest 4 bits must be set, see
    // section 10.9) and enable the LAPIC.
    g_lapic_regs->sivr_bit.sv = 47;
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
        g_ioapic_id = ics_ioapic->ioapic_id;
    } else {
        kprintf("apic: could not get I/O APIC ICS from acpi, using 0x%08X\n",
                acpiIOAPIC_FALLBACK_ADDR);
        g_ioapic_regs = (ioapic_regs_t *)acpiIOAPIC_FALLBACK_ADDR;

        uint32_t reg_val;
        reg_val = prv_ioapic_read_u32(IOAPIC_REG_ID);
        g_ioapic_id = (reg_val >> 24) & 0xF;
    }

    uint32_t ver_redir = prv_ioapic_read_u32(IOAPIC_REG_VER_REDIR);
    g_ioapic_version = ver_redir & 0xFF;
    g_ioapic_redirs = 1 + ((ver_redir >> 16) & 0xFF);

    const ioapic_redir_t redir0 = {
        .intvec = 32,
        .delmod = IOAPIC_DELMOD_FIXED,
        .destmod = IOAPIC_DESTMOD_PHYSICAL,
        .intpol = IOAPIC_INTPOL_ACTIVE_HIGH,
        .trigmod = IOAPIC_TRIGMOD_EDGE,
        .intmask = 0,
        .apicid = g_lapic_regs->lapic_id_bit.apic_id,
    };
    uint64_t redir0_val;
    kmemcpy(&redir0_val, &redir0, 8);
    prv_ioapic_write_u64(IOAPIC_REG_REDIR(0), redir0_val);

    kprintf("apic: I/O APIC 0x%02X version %u (%u entries) at %P\n",
            g_ioapic_id, g_ioapic_version, g_ioapic_redirs, g_ioapic_regs);
    return true;
}

static uint32_t prv_ioapic_read_u32(ioapic_reg_sel_t regsel) {
    g_ioapic_regs->regsel = regsel;
    return g_ioapic_regs->win;
}

[[maybe_unused]]
static void prv_ioapic_write_u32(ioapic_reg_sel_t regsel, uint32_t val) {
    g_ioapic_regs->regsel = regsel;
    g_ioapic_regs->win = val;
}

static void prv_ioapic_write_u64(ioapic_reg_sel_t regsel, uint64_t val) {
    g_ioapic_regs->regsel = regsel;
    g_ioapic_regs->win = (uint32_t)val;

    g_ioapic_regs->regsel = regsel;
    g_ioapic_regs->win = val >> 32;
}
