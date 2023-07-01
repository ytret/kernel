#include <gdt.h>
#include <printf.h>
#include <vga.h>

void
main (void)
{
    vga_clear();
    printf("Hello, world!\n");

    gdt_init();

    for (;;)
    {}
}
