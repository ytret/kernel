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
#include "smp.h"
#include "taskmgr.h"
#include "term.h"
#include "vmm.h"

#define MULTIBOOT_MAGIC_NUM 0x2BADB002

static void check_bootloader(uint32_t magic_num, uint32_t mbi_addr);

void main(uint32_t magic_num, uint32_t mbi_addr) {
    mbi_init(mbi_addr);

    term_init();

    kprintf("Hello, world!\n");
    check_bootloader(magic_num, mbi_addr);

    gdtr_t gdtr;
    gdt_init_pre_smp(&gdtr);
    gdt_load(&gdtr);

    idt_init();

    heap_init();
    mbi_save_on_heap();

    term_init_history();
    // term_clear();

    acpi_init();
    lapic_init(true);
    ioapic_init();

    pit_init(PIT_PERIOD_MS);
    ioapic_map_irq(PIT_IRQ, 32 + PIT_IRQ, lapic_get_id());

    kbd_init();
    ioapic_map_irq(KBD_IRQ, 32 + KBD_IRQ, lapic_get_id());

    vmm_init();
    pmm_init();

    lapic_map_pages();
    ioapic_map_pages();

    taskmgr_global_init();

    __asm__ volatile("sti");
    kprintf("Interrupts enabled\n");

    lapic_calib_tim();

    smp_init();
    // NOTE: main() is executed only by the bootstrap processor (BSP). Hence,
    // everything below is also executed only by the BSP.

    devmgr_init();

    taskmgr_local_init(init_bsp_task);
    kprintf("End of main\n");
}

static void check_bootloader(uint32_t magic_num, uint32_t mbi_addr) {
    if (MULTIBOOT_MAGIC_NUM == magic_num) {
        kprintf("Booted by a multiboot-compliant bootloader\n");
        kprintf("Multiboot information structure is at %P\n", mbi_addr);
    } else {
        panic_enter();
        kprintf("Magic number: 0x%X, expected: 0x%X\n", magic_num,
                MULTIBOOT_MAGIC_NUM);
        panic("booted by an unknown bootloader");
    }
}
