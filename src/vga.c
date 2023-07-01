#include <panic.h>
#include <vga.h>

#include <stddef.h>
#include <stdint.h>

#define VGA_MEMORY_ADDR         0xB8000
#define MAX_ROWS                25
#define MAX_COLS                80

static uint16_t * const gp_vga_memory = (uint16_t *) VGA_MEMORY_ADDR;

static uint8_t g_row;
static uint8_t g_col;

static void put_char(char ch);
static void put_char_at(uint8_t row, uint8_t col, char ch);

void
vga_clear (void)
{
    g_row = 0;
    g_col = 0;
    __builtin_memset(gp_vga_memory, 0, (2 * MAX_ROWS * MAX_COLS));
}

void
vga_print_str (char const * p_str)
{
    while (*p_str != 0)
    {
        char ch = *p_str;
        put_char(ch);
        p_str++;
    }
}

static void
put_char (char ch)
{
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
