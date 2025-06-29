#include <stdint.h>

#include "acpi/acpi.h"
#include "acpi/apic.h"
#include "blkdev/blkdev.h"
#include "devmgr.h"
#include "gdt.h"
#include "gpt.h"
#include "heap.h"
#include "idt.h"
#include "kbd.h"
#include "kprintf.h"
#include "kshell/kshell.h"
#include "mbi.h"
#include "panic.h"
#include "pit.h"
#include "pmm.h"
#include "taskmgr.h"
#include "term.h"
#include "vmm.h"

#define MULTIBOOT_MAGIC_NUM 0x2BADB002

static void check_bootloader(uint32_t magic_num, uint32_t mbi_addr);
[[gnu::noreturn]] static void init_entry(void);

void main(uint32_t magic_num, uint32_t mbi_addr) {
    mbi_init(mbi_addr);

    term_init();

    kprintf("Hello, world!\n");
    check_bootloader(magic_num, mbi_addr);

    gdt_init();
    idt_init();

    heap_init();
    mbi_save_on_heap();

    term_init_history();
    // term_clear();

    acpi_init();
    apic_init();
    pit_init(PIT_PERIOD_MS);
    apic_map_irq(PIT_IRQ, 32 + PIT_IRQ);
    __asm__ volatile("sti");
    kprintf("Interrupts enabled\n");

    kbd_init();
    apic_map_irq(KBD_IRQ, 32 + KBD_IRQ);

    vmm_init();
    pmm_init();
    apic_map_pages();

    devmgr_init();

    /*
    uint64_t root_start_sector;
    uint64_t root_num_sectors;
    bool b_root_found =
        gpt_find_root_part(&root_start_sector, &root_num_sectors);
    if (!b_root_found) { kprintf("Could not find root partition\n"); }
    */

    taskmgr_init(init_entry);

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

[[gnu::noreturn]]
static void init_entry(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    __asm__ volatile("sti");

    taskmgr_new_kernel_task((uint32_t)blkdev_task_entry);

    kshell();

    panic_enter();
    kprintf("init_entry: kshell returned\n");
    panic("unexpected behavior");
}
