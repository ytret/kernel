#include <printf.h>
#include <vga.h>

void
printf (char const * p_msg)
{
    vga_print_str(p_msg);
}
