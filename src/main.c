#include <gdt.h>
#include <idt.h>
#include <pic.h>
#include <pit.h>
#include <printf.h>
#include <vga.h>

void
main (void)
{
    vga_clear();
    printf("Hello, world!\n");

    gdt_init();
    idt_init();
    pic_init();

    pit_init(PIT_PERIOD_MS);
    pic_set_mask(PIT_IRQ, false);

    __asm__ volatile ("sti");
    printf("Interrupts enabled.\n");

    for (;;)
    {}
}
