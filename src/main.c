#include <gdt.h>
#include <idt.h>
#include <panic.h>
#include <pic.h>
#include <pit.h>
#include <pmm.h>
#include <printf.h>
#include <vga.h>
#include <vmm.h>

#include <stdint.h>

#define MULTIBOOT_MAGIC_NUM	0x2BADB002

void
main (uint32_t magic_num, mbi_t const * p_mbi)
{
    vga_clear();
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

    pmm_init(p_mbi);
    vmm_init();
}
