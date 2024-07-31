#include "framebuf.h"
#include "mbi.h"
#include "term.h"
#include "vga.h"

typedef struct {
    void (*p_put_char_at)(size_t row, size_t col, char ch);
    void (*p_put_cursor_at)(size_t row, size_t col);
    void (*p_scroll)(void);
} output_impl_t;

static output_impl_t g_output_impl;
static term_kbd_handler_t g_kbd_handler;

static size_t g_max_row;
static size_t g_max_col;

static size_t g_row;
static size_t g_col;

static void put_char(char ch);

void term_init(void) {
    mbi_t const *p_mbi = mbi_ptr();

    if ((p_mbi->flags & MBI_FLAG_FRAMEBUF) &&
        (MBI_FRAMEBUF_EGA != p_mbi->framebuffer_type)) {
        framebuf_init();

        g_max_row = framebuf_height_chars();
        g_max_col = framebuf_width_chars();

        g_output_impl.p_put_char_at = framebuf_put_char_at;
        g_output_impl.p_put_cursor_at = framebuf_put_cursor_at;
        g_output_impl.p_scroll = framebuf_scroll;
    } else {
        vga_init();

        g_max_row = vga_height_chars();
        g_max_col = vga_width_chars();

        g_output_impl.p_put_char_at = vga_put_char_at;
        g_output_impl.p_put_cursor_at = vga_put_cursor_at;
        g_output_impl.p_scroll = vga_scroll;
    }
}

void term_clear(void) {
    term_clear_rows(0, g_max_row);
}

void term_clear_rows(size_t start_row, size_t num_rows) {
    for (size_t row = start_row; row < (start_row + num_rows); row++) {
        for (size_t col = 0; col < g_max_col; col++) {
            g_output_impl.p_put_char_at(row, col, ' ');
        }
    }

    term_put_cursor_at(0, 0);
}

void term_print_str(char const *p_str) {
    while ((*p_str) != 0) {
        char ch = (*p_str);
        put_char(ch);
        p_str++;
    }

    term_put_cursor_at(g_row, g_col);
}

void term_print_str_len(char const *p_str, size_t len) {
    for (size_t idx = 0; idx < len; idx++) {
        put_char(p_str[idx]);
    }

    term_put_cursor_at(g_row, g_col);
}

void term_put_char_at(size_t row, size_t col, char ch) {
    g_output_impl.p_put_char_at(row, col, ch);
}

void term_put_cursor_at(size_t row, size_t col) {
    g_output_impl.p_put_cursor_at(row, col);

    g_row = row;
    g_col = col;
}

size_t term_row(void) {
    return g_row;
}

size_t term_col(void) {
    return g_col;
}

size_t term_height(void) {
    return g_max_row;
}

size_t term_width(void) {
    return g_max_col;
}

void term_kbd_callback(uint8_t key, bool b_released) {
    if (g_kbd_handler.p_event_handler) {
        g_kbd_handler.p_event_handler(key, b_released);
    }
}

void term_attach_kbd_handler(term_kbd_handler_t handler) {
    g_kbd_handler = handler;
}

static void put_char(char ch) {
    switch (ch) {
    case '\n':
        g_col = 0;
        g_row++;
        if (g_row >= g_max_row) {
            g_row = (g_max_row - 1);
            g_output_impl.p_scroll();
        }
        break;

    default:
        g_output_impl.p_put_char_at(g_row, g_col, ch);

        g_col++;
        if (g_col >= g_max_col) {
            g_col = 0;
            g_row++;
            if (g_row >= g_max_row) {
                g_row = (g_max_row - 1);
                g_output_impl.p_scroll();
            }
        }
    }
}
