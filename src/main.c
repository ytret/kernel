#include <printf.h>
#include <vga.h>

#include <stdint.h>

void
main (void)
{
    vga_clear();
    printf("Hello, World!");

    for (;;)
    {}
}
