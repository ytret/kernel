#include <alloc.h>
#include <gdt.h>
#include <idt.h>
#include <kbd.h>
#include <kshell/kshell.h>
#include <panic.h>
#include <pic.h>
#include <pit.h>
#include <pmm.h>
#include <printf.h>
#include <taskmgr.h>
#include <term.h>
#include <vmm.h>

#include <stdint.h>

#define MULTIBOOT_MAGIC_NUM	0x2BADB002

static void init_entry(void) __attribute__ ((noreturn));

void
main (uint32_t magic_num, mbi_t const * p_mbi)
{
    term_init();
    term_clear();
    printf("Hello, world!\n");

    if (MULTIBOOT_MAGIC_NUM == magic_num)
    {
	printf("Booted by a multiboot-compliant bootloader\n");
	printf("Multiboot information structure is at %P\n", p_mbi);
    }
    else
    {
	printf("Magic number: 0x%X, expected: 0x%X\n",
	       magic_num, MULTIBOOT_MAGIC_NUM);
	panic("booted by an unknown bootloader");
    }

    gdt_init();
    idt_init();
    pic_init();

    pit_init(PIT_PERIOD_MS);
    pic_set_mask(PIT_IRQ, false);

    __asm__ volatile ("sti");
    printf("Interrupts enabled\n");

    alloc_init(((void *) VMM_HEAP_START), VMM_HEAP_SIZE);

    pmm_init(p_mbi);
    vmm_init(p_mbi);

    mbi_copy(p_mbi);

    pic_set_mask(KBD_IRQ, false);

    taskmgr_init();
    taskmgr_start_scheduler(init_entry);

    printf("End of main\n");
}

__attribute__ ((noreturn))
static void
init_entry (void)
{
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    //
    __asm__ ("sti");

    kshell();

    printf("init_entry: kshell returned\n");
    panic("unexpected behavior");
}
