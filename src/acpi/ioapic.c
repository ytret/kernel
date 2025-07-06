/**
 * @file ioapic.c
 * Input/Output Advanced Programmable Interrupt Controller (I/O APIC, IOAPIC)
 * driver.
 */

#include "acpi/acpi.h"
#include "acpi/ioapic.h"
#include "kprintf.h"
#include "memfun.h"
#include "pic.h"
#include "vmm.h"

static ioapic_regs_t *g_ioapic_regs;

static uint8_t g_ioapic_id;
static uint8_t g_ioapic_version;
static uint32_t g_ioapic_redirs;

static void prv_ioapic_read_u32(ioapic_reg_sel_t regsel, void *buf);
static void prv_ioapic_read_u64(ioapic_reg_sel_t regsel, void *buf);
static void prv_ioapic_write_u32(ioapic_reg_sel_t regsel, const void *buf);
static void prv_ioapic_write_u64(ioapic_reg_sel_t regsel, const void *buf);

void ioapic_init(void) {
    pic_mask_all();

    const acpi_ic_ioapic_t *const ics_ioapic = acpi_get_ioapic_ics();
    if (ics_ioapic) {
        g_ioapic_regs = (ioapic_regs_t *)ics_ioapic->ioapic_addr;
    } else {
        kprintf("ioapic: could not get I/O APIC ICS from acpi, using 0x%08X\n",
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

    kprintf("ioapic: I/O APIC 0x%02X version %u (%u entries) at %P\n",
            g_ioapic_id, g_ioapic_version, g_ioapic_redirs, g_ioapic_regs);
}

void ioapic_map_pages(void) {
    const uint32_t ioapic_first_page = (uint32_t)g_ioapic_regs;
    constexpr size_t ioapic_num_pages = (sizeof(*g_ioapic_regs) + 4095) / 4096;
    for (uint32_t idx = 0; idx < ioapic_num_pages; idx++) {
        const uint32_t ioapic_page = ioapic_first_page + 4096 * idx;
        vmm_map_kernel_page(ioapic_page, ioapic_page);
    }
}

bool ioapic_map_irq(uint8_t irq_num, uint8_t vec_num, uint8_t lapic_id) {
    if (!g_ioapic_regs) {
        kprintf("ioapic: cannot remap IRQ %u to vector %u: I/O APIC is not "
                "initialized\n",
                irq_num, vec_num);
        return false;
    }

    const acpi_irq_remap_t *const irq_remap = acpi_find_irq_remap(irq_num);
    const uint32_t gsi_num = irq_remap ? irq_remap->gsi : irq_num;

    const ioapic_redir_t redir = {
        .intvec = vec_num,
        .delmod = IOAPIC_DELMOD_FIXED,
        .destmod = APIC_DESTMOD_PHYSICAL,
        .intpol = IOAPIC_INTPOL_ACTIVE_HIGH,
        .trigmod = APIC_TRIGMOD_EDGE,
        .intmask = 0,
        .apicid = lapic_id,
    };
    bool ok = ioapic_set_redirect(gsi_num, &redir);
    if (ok) {
        kprintf("ioapic: mapped IRQ %u to vector %u of LAPIC ID %u\n", irq_num,
                vec_num, lapic_id);
    }
    return ok;
}

bool ioapic_set_redirect(uint32_t gsi, const ioapic_redir_t *redir) {
    if (!g_ioapic_regs) {
        kprintf(
            "ioapic: cannot set I/O APIC GSI %u: I/O APIC is not initialized\n",
            gsi);
        return false;
    }
    if (gsi >= g_ioapic_redirs) {
        kprintf("ioapic: cannot set I/O APIC GSI %u: maximum GSI is %u\n", gsi,
                g_ioapic_redirs - 1);
        return false;
    }

    ioapic_redir_t prev_val;
    prv_ioapic_read_u64(IOAPIC_REG_REDIR(gsi), &prev_val);
    if (prev_val.intvec != 0) {
        kprintf("ioapic: cannot set I/O APIC GSI %u: already mapped to %u\n",
                prev_val.intvec);
        return false;
    }

    prv_ioapic_write_u64(IOAPIC_REG_REDIR(gsi), redir);

    // FIXME: read the value back and check if the r/w fields are the same as
    // in 'redir'?

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
