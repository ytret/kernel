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

static uint8_t g_row;
static uint8_t g_col;

static void put_char(char ch);
static void put_char_at(uint8_t row, uint8_t col, char ch);
static void put_cursor_at(uint8_t row, uint8_t col);
static void scroll(void);

void
vga_clear (void)
{
    for (size_t row = 0; row < MAX_ROWS; row++)
    {
        for (size_t col = 0; col < MAX_COLS; col++)
        {
            put_char_at(row, col, ' ');
        }
    }
}

void
vga_print_str (char const * p_str)
{
    while ((*p_str) != 0)
    {
        char ch = (*p_str);
        put_char(ch);
        p_str++;
    }

    put_cursor_at(g_row, g_col);
}

void
vga_print_str_len (char const * p_str, size_t len)
{
    for (size_t idx = 0; idx < len; idx++)
    {
        put_char(p_str[idx]);
    }

    put_cursor_at(g_row, g_col);
}

static void
put_char (char ch)
{
    switch (ch)
    {
        case '\n':
            g_col = 0;
            g_row++;
            if (g_row >= MAX_ROWS)
            {
                g_row = (MAX_ROWS - 1);
                scroll();
            }
        break;

        default:
            put_char_at(g_row, g_col, ch);

            g_col++;
            if (g_col >= MAX_COLS)
            {
                g_col = 0;
                g_row++;
                if (g_row >= MAX_ROWS)
                {
                    g_row = 0;
                }
            }
    }
}

static void
put_char_at (uint8_t row, uint8_t col, char ch)
{
    if ((row >= MAX_ROWS) || (col >= MAX_COLS))
    {
        panic();
    }

    size_t idx = ((row * MAX_COLS) + col);
    gp_vga_memory[idx] = ((0x0F << 8) | ch);
}

static void
put_cursor_at (uint8_t row, uint8_t col)
{
    size_t char_idx = (row * MAX_COLS + col);
    port_outb(PORT_CRTC_ADDR, REG_CRTC_CURSOR_LOC_HI);
    port_outb(PORT_CRTC_DATA, ((uint8_t) (char_idx >> 8)));
    port_outb(PORT_CRTC_ADDR, REG_CRTC_CURSOR_LOC_LO);
    port_outb(PORT_CRTC_DATA, ((uint8_t) char_idx));
}

static void
scroll (void)
{
    // Bury the first row.
    //
    __builtin_memmove(gp_vga_memory,
                      &gp_vga_memory[1 * MAX_COLS],
                      (2 * (MAX_ROWS - 1) * MAX_COLS));

    // Clean the last row.
    //
    __builtin_memset(&gp_vga_memory[(MAX_ROWS - 1) * MAX_COLS], 0,
                     2 * MAX_COLS);
}
