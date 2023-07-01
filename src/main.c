#include <printf.h>
#include <vga.h>

void
main (void)
{
    vga_clear();
    printf("Hello, World!");

    for (;;)
    {}
}
