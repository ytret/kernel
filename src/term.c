#include <term.h>
#include <vga.h>

#define MAX_ROWS        25
#define MAX_COLS        80

static uint8_t g_row;
static uint8_t g_col;

static void put_char(char ch);

void
term_init (void)
{
    vga_init();
}

void
term_clear (void)
{
    term_clear_rows(0, MAX_ROWS);
}

void
term_clear_rows (size_t start_row, size_t num_rows)
{
    for (size_t row = start_row; row < (start_row + num_rows); row++)
    {
        for (size_t col = 0; col < MAX_COLS; col++)
        {
            vga_put_char_at(row, col, ' ');
        }
    }

    term_put_cursor_at(0, 0);
}

void
term_print_str (char const * p_str)
{
    while ((*p_str) != 0)
    {
        char ch = (*p_str);
        put_char(ch);
        p_str++;
    }

    term_put_cursor_at(g_row, g_col);
}

void
term_print_str_len (char const * p_str, size_t len)
{
    for (size_t idx = 0; idx < len; idx++)
    {
        put_char(p_str[idx]);
    }

    term_put_cursor_at(g_row, g_col);
}

void
term_put_char_at (size_t row, size_t col, char ch)
{
    vga_put_char_at(row, col, ch);
}

void
term_put_cursor_at (size_t row, size_t col)
{
    vga_put_cursor_at(row, col);

    g_row = row;
    g_col = col;
}

size_t
term_row (void)
{
    return (g_row);
}

size_t
term_col (void)
{
    return (g_col);
}

size_t
term_height (void)
{
    return (MAX_ROWS);
}

size_t
term_width (void)
{
    return (MAX_COLS);
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
                vga_scroll();
            }
        break;

        default:
            vga_put_char_at(g_row, g_col, ch);

            g_col++;
            if (g_col >= MAX_COLS)
            {
                g_col = 0;
                g_row++;
                if (g_row >= MAX_ROWS)
                {
                    g_row = (MAX_ROWS - 1);
		    vga_scroll();
                }
            }
    }
}
