#include <gdt.h>
#include <idt.h>
#include <pic.h>
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

    __asm__ volatile ("sti");
    printf("Interrupts enabled.\n");

    for (;;)
    {}
}
