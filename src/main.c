#include <stdint.h>

#include "acpi/acpi.h"
#include "acpi/ioapic.h"
#include "acpi/lapic.h"
#include "devmgr.h"
#include "gdt.h"
#include "heap.h"
#include "idt.h"
#include "init.h"
#include "kbd.h"
#include "kprintf.h"
#include "mbi.h"
#include "panic.h"
#include "pit.h"
#include "pmm.h"
#include "serial.h"
#include "smp.h"
#include "taskmgr.h"
#include "term.h"
#include "vmm.h"

#define MULTIBOOT_MAGIC_NUM 0x2BADB002

// See link.ld.
extern uint32_t ld_vmm_kernel_start;
extern uint32_t ld_vmm_kernel_end;

static pmm_mmap_t g_mmap;
static pmm_region_t g_kernel_region;

static void check_bootloader(uint32_t magic_num, uint32_t mbi_addr);
static uint32_t prv_main_find_first_free_page(void);
static void prv_main_add_kernel_region(pmm_mmap_t *mmap, uintptr_t region_start,
                                       uintptr_t region_end);

void main(uint32_t magic_num, uint32_t mbi_addr) {
    mbi_init(mbi_addr);

    serial_init();
    term_init();

    kprintf("main: Hello, world!\n");
    check_bootloader(magic_num, mbi_addr);

    gdtr_t gdtr;
    gdt_init_pre_smp(&gdtr);
    gdt_load(&gdtr);

    idt_init();

    term_init_history();
    term_clear();

    if (!mbi_fill_mmap(mbi_ptr(), &g_mmap)) {
        panic("failed to fill the memory map");
    }
    prv_main_add_kernel_region(&g_mmap, (uint32_t)&ld_vmm_kernel_start,
                               prv_main_find_first_free_page());
    pmm_init(&g_mmap);

    heap_init();
    mbi_save_on_heap();

    acpi_init();
    lapic_init(true);
    ioapic_init();

    pit_init(PIT_PERIOD_MS);
    if (!ioapic_map_irq(PIT_IRQ, 32 + PIT_IRQ, lapic_get_id())) {
        panic("failed to map PIT IRQ");
    }

    kbd_init();
    if (!ioapic_map_irq(KBD_IRQ, 32 + KBD_IRQ, lapic_get_id())) {
        panic("failed to map kbd IRQ");
    }

    vmm_init();

    lapic_map_pages();
    ioapic_map_pages();

    taskmgr_global_init();

    __asm__ volatile("sti");
    kprintf("main: interrupts enabled\n");

    lapic_calib_tim();

    smp_init();
    // NOTE: main() is executed only by the bootstrap processor (BSP). Hence,
    // everything below is also executed only by the BSP.

    devmgr_init();

    taskmgr_local_init(init_bsp_task);
    kprintf("main: end of main\n");
}

static void check_bootloader(uint32_t magic_num, uint32_t mbi_addr) {
    if (MULTIBOOT_MAGIC_NUM == magic_num) {
        kprintf("main: booted by a multiboot-compliant bootloader\n");
        kprintf("main: multiboot information structure is at %P\n", mbi_addr);
    } else {
        panic_enter();
        kprintf("main: magic number: 0x%X, expected: 0x%X\n", magic_num,
                MULTIBOOT_MAGIC_NUM);
        panic("booted by an unknown bootloader");
    }
}

static uint32_t prv_main_find_first_free_page(void) {
    const uint32_t kernel_end = (uint32_t)&ld_vmm_kernel_end;

    uint32_t last_used_addr;
    const mbi_mod_t *const last_mod = mbi_last_mod();
    if (last_mod) { last_used_addr = last_mod->mod_end; }
    if (!last_mod || kernel_end > last_used_addr) {
        last_used_addr = kernel_end;
    }

    return (last_used_addr + 0x3FFFFF) & ~(0X3FFFFF);
}

static void prv_main_add_kernel_region(pmm_mmap_t *mmap, uintptr_t region_start,
                                       uintptr_t region_end_excl) {
    if (region_end_excl == 0) {
        panic("argument 'region_end_excl' value must not be 0");
    }

    const uintptr_t region_end_incl = region_end_excl - 1;

    // It is assumed that [region_start, region_end_excl) are residing in a
    // single memory map entry. Otherwise this function will fail.
    pmm_region_t *found_region;
    LIST_FIND(&mmap->entry_list, found_region, pmm_region_t, node,
              region->start <= region_start &&
                  region_end_incl <= region->end_incl,
              region);
    if (!found_region) {
        kprintf("main: could not find a single region that covers [0x%08x; "
                "0x%08x)\n",
                region_start, region_end_excl);
        panic("not implemented");
    }

    g_kernel_region.start = region_start;
    g_kernel_region.end_incl = region_end_incl;
    g_kernel_region.type = PMM_REGION_KERNEL_AND_MODS;

    kprintf("main: added kernel region 0x%08x_%08x .. 0x%08x_%08x\n",
            (uint32_t)(g_kernel_region.start >> 32),
            (uint32_t)g_kernel_region.start,
            (uint32_t)(g_kernel_region.end_incl >> 32),
            (uint32_t)g_kernel_region.end_incl);

    if (found_region->start < region_start) {
        // [original region]
        //    [   kernel   ]
        found_region->end_incl = region_start - 1;
        list_insert(&mmap->entry_list, &found_region->node,
                    &g_kernel_region.node);
    } else if (found_region->end_incl > region_end_incl) {
        // [original region]
        // [kernel]
        found_region->start = region_end_excl;
        list_insert(&mmap->entry_list, found_region->node.p_prev,
                    &g_kernel_region.node);
    } else {
        // [original region]
        // [    kernel     ]
        list_insert(&mmap->entry_list, &found_region->node,
                    &g_kernel_region.node);
        list_remove(&mmap->entry_list, &found_region->node);
    }
}
