#include <stdint.h>

#include "gdt.h"
#include "gpt.h"
#include "heap.h"
#include "idt.h"
#include "kbd.h"
#include "kprintf.h"
#include "kshell/kshell.h"
#include "mbi.h"
#include "panic.h"
#include "pci.h"
#include "pic.h"
#include "pit.h"
#include "pmm.h"
#include "taskmgr.h"
#include "term.h"
#include "vmm.h"

#define MULTIBOOT_MAGIC_NUM 0x2BADB002

static void init_entry(void) __attribute__((noreturn));

void main(uint32_t magic_num, uint32_t mbi_addr) {
    mbi_init(mbi_addr);

    term_init();
    kprintf("Hello, world!\n");

    if (MULTIBOOT_MAGIC_NUM == magic_num) {
        kprintf("Booted by a multiboot-compliant bootloader\n");
        kprintf("Multiboot information structure is at %P\n", mbi_addr);
    } else {
        kprintf("Magic number: 0x%X, expected: 0x%X\n", magic_num,
                MULTIBOOT_MAGIC_NUM);
        panic("booted by an unknown bootloader");
    }

    gdt_init();
    idt_init();
    pic_init();

    pit_init(PIT_PERIOD_MS);
    pit_enable_interrupt();

    __asm__ volatile("sti");
    kprintf("Interrupts enabled\n");

    heap_init();
    mbi_save_on_heap();

    kbd_init();

    vmm_init();
    pmm_init();

    pci_init();

    uint64_t root_start_sector;
    uint64_t root_num_sectors;
    bool b_root_found =
        gpt_find_root_part(&root_start_sector, &root_num_sectors);
    if (!b_root_found) { kprintf("Could not find root partition\n"); }

    taskmgr_init(init_entry);

    kprintf("End of main\n");
}

__attribute__((noreturn)) static void init_entry(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    __asm__("sti");

    kshell();

    kprintf("init_entry: kshell returned\n");
    panic("unexpected behavior");
}
