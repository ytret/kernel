#include <stdint.h>

#define VGA_MEMORY_ADDR         0xB8000

static uint16_t * const gp_vga_memory = (uint16_t *) VGA_MEMORY_ADDR;

void
main (void)
{
    gp_vga_memory[0] = 0xA141;

    for (;;)
    {}
}
