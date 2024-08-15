#include "framebuf.h"
#include "mbi.h"
#include "mutex.h"
#include "panic.h"
#include "queue.h"
#include "term.h"
#include "vga.h"

typedef struct {
    void (*p_put_char_at)(size_t row, size_t col, char ch);
    void (*p_put_cursor_at)(size_t row, size_t col);
    void (*p_clear_rows)(size_t start_row, size_t num_rows);
    void (*p_scroll)(void);
} output_impl_t;

static task_mutex_t g_mutex;
static bool gb_panic_mode;
static output_impl_t g_output_impl;

static size_t g_max_row;
static size_t g_max_col;

static size_t g_row;
static size_t g_col;

static void put_char(char ch);

__attribute__((artificial)) static inline void assert_owns_mutex(void) {
    if (!mutex_caller_owns(&g_mutex) && !gb_panic_mode) { panic_silent(); }
}

__attribute__((artificial)) static inline void put_cursor_at(size_t row,
                                                             size_t col) {
    g_output_impl.p_put_cursor_at(row, col);
    g_row = row;
    g_col = col;
}

void term_init(void) {
    mbi_t const *p_mbi = mbi_ptr();

    if ((p_mbi->flags & MBI_FLAG_FRAMEBUF) &&
        (MBI_FRAMEBUF_EGA != p_mbi->framebuffer_type)) {
        framebuf_init();

        g_max_row = framebuf_height_chars();
        g_max_col = framebuf_width_chars();

        g_output_impl.p_put_char_at = framebuf_put_char_at;
        g_output_impl.p_put_cursor_at = framebuf_put_cursor_at;
        g_output_impl.p_clear_rows = framebuf_clear_rows;
        g_output_impl.p_scroll = framebuf_scroll;
    } else {
        vga_init();

        g_max_row = vga_height_chars();
        g_max_col = vga_width_chars();

        g_output_impl.p_put_char_at = vga_put_char_at;
        g_output_impl.p_put_cursor_at = vga_put_cursor_at;
        g_output_impl.p_clear_rows = vga_clear_rows;
        g_output_impl.p_scroll = vga_scroll;
    }

    mutex_init(&g_mutex);
    term_clear();
}

void term_acquire_mutex(void) {
    if (!gb_panic_mode) { mutex_acquire(&g_mutex); }
}

void term_release_mutex(void) {
    if (!gb_panic_mode) { mutex_release(&g_mutex); }
}

bool term_owns_mutex(void) {
    return mutex_caller_owns(&g_mutex);
}

void term_enter_panic_mode(void) {
    gb_panic_mode = true;
}

void term_clear(void) {
    assert_owns_mutex();
    g_output_impl.p_clear_rows(0, g_max_row);
    put_cursor_at(0, 0);
}

void term_clear_rows(size_t start_row, size_t num_rows) {
    assert_owns_mutex();
    g_output_impl.p_clear_rows(start_row, num_rows);
}

void term_print_str(char const *p_str) {
    assert_owns_mutex();
    while ((*p_str) != 0) {
        char ch = (*p_str);
        put_char(ch);
        p_str++;
    }
    term_put_cursor_at(g_row, g_col);
}

void term_print_str_len(char const *p_str, size_t len) {
    assert_owns_mutex();
    for (size_t idx = 0; idx < len; idx++) {
        put_char(p_str[idx]);
    }
    put_cursor_at(g_row, g_col);
}

void term_put_char_at(size_t row, size_t col, char ch) {
    assert_owns_mutex();
    g_output_impl.p_put_char_at(row, col, ch);
}

void term_put_cursor_at(size_t row, size_t col) {
    assert_owns_mutex();
    put_cursor_at(row, col);
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

void term_read_kbd_event(kbd_event_t *p_event) {
    queue_read(kbd_event_queue(), p_event, sizeof(kbd_event_t));
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
