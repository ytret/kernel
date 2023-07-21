#include <alloc.h>
#include <gdt.h>
#include <idt.h>
#include <kbd.h>
#include <kshell.h>
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

void
main (uint32_t magic_num, mbi_t const * p_mbi)
{
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

    pmm_init(p_mbi);
    vmm_init();
    alloc_init(((void *) VMM_HEAP_START), VMM_HEAP_SIZE);

    if (p_mbi->flags & (1 << 3))
    {
        printf("mods_count = %u\n", p_mbi->mods_count);
        printf("mods_addr = %P\n", p_mbi->mods_addr);

        for (size_t idx = 0; idx < p_mbi->mods_count; idx++)
        {
            mbi_mod_t const * p_mod = ((mbi_mod_t const *) p_mbi->mods_addr);

            printf("Module %d: start = %P, end = %P", idx, p_mod->mod_start,
                   p_mod->mod_end);
            if (p_mod->string)
            {
                printf(", string = '%s'", p_mod->string);
            }
            printf("\n");

            p_mod += 1;
        }
    }

    pic_set_mask(KBD_IRQ, false);

    printf("Initializing task manager\n");
    taskmgr_init();

    kshell_init();

    printf("End of main\n");
}
