#include <panic.h>
#include <port.h>
#include <vga.h>

#include <stddef.h>
#include <stdint.h>

#define VGA_MEMORY_ADDR         0xB8000

#define MAX_ROWS                25
#define MAX_COLS                80

#define PORT_CRTC_ADDR          ((uint16_t) 0x3D4)
#define PORT_CRTC_DATA          ((uint16_t) 0x3D5)

#define REG_CRTC_CURSOR_LOC_HI  ((uint8_t) 0x0E)
#define REG_CRTC_CURSOR_LOC_LO  ((uint8_t) 0x0F)

static uint16_t * const gp_vga_memory = (uint16_t *) VGA_MEMORY_ADDR;

void
vga_put_char_at (uint8_t row, uint8_t col, char ch)
{
    if ((row >= MAX_ROWS) || (col >= MAX_COLS))
    {
        panic_silent();
    }

    size_t idx = ((row * MAX_COLS) + col);
    gp_vga_memory[idx] = ((0x0F << 8) | ch);
}

void
vga_put_cursor_at (uint8_t row, uint8_t col)
{
    if ((row >= MAX_ROWS) || (col >= MAX_COLS))
    {
        panic_silent();
    }

    size_t char_idx = (row * MAX_COLS + col);
    port_outb(PORT_CRTC_ADDR, REG_CRTC_CURSOR_LOC_HI);
    port_outb(PORT_CRTC_DATA, ((uint8_t) (char_idx >> 8)));
    port_outb(PORT_CRTC_ADDR, REG_CRTC_CURSOR_LOC_LO);
    port_outb(PORT_CRTC_DATA, ((uint8_t) char_idx));
}

void
vga_scroll (void)
{
    // Bury the first row.
    //
    __builtin_memmove(gp_vga_memory,
                      &gp_vga_memory[1 * MAX_COLS],
                      (2 * (MAX_ROWS - 1) * MAX_COLS));

    // Clean the last row.
    //
    for (size_t col = 0; col < MAX_COLS; col++)
    {
        vga_put_char_at((MAX_ROWS - 1), col, ' ');
    }
}
