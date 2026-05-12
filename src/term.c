#include "framebuf.h"
#include "kbd.h"
#include "kmutex.h"
#include "log.h"
#include "mbi.h"
#include "panic.h"
#include "queue.h"
#include "term.h"
#include "vga.h"

typedef struct {
    void (*p_init)(void);
    void (*p_map_iomem)(void);

    void (*p_put_char_at)(size_t row, size_t col, char ch);
    void (*p_put_cursor_at)(size_t row, size_t col);
    void (*p_clear_rows)(size_t start_row, size_t num_rows);
    void (*p_scroll_new_row)(void);
} output_impl_t;

/**
 * The terminal mutex.
 *
 * To maintain coherent output in the terminal, each task that wants to print
 * must lock the terminal mutex. See #term_acquire_mutex(),
 * #term_release_mutex(), #term_owns_mutex().
 */
static task_mutex_t g_term_mutex;

static bool gb_panic_mode;
static output_impl_t g_output_impl;
static bool g_term_has_impl;

static size_t g_max_row;
static size_t g_max_col;

static size_t g_row;
static size_t g_col;

static void put_char(char ch);
static void scroll_new_row(void);

[[gnu::artificial]]
static inline void assert_owns_mutex(void) {
    if (!mutex_caller_owns(&g_term_mutex) && !gb_panic_mode) { panic_nested(); }
}

[[gnu::artificial]]
static inline void put_cursor_at(size_t row, size_t col) {
    g_output_impl.p_put_cursor_at(row, col);
    g_row = row;
    g_col = col;
}

void term_early_init(void) {
    mbi_t const *p_mbi = mbi_ptr();

    if ((p_mbi->flags & MBI_FLAG_FRAMEBUF) &&
        (MBI_FRAMEBUF_EGA != p_mbi->framebuffer_type)) {
        LOG_DEBUG("terminal type - framebuffer");

        framebuf_early_init();

        g_max_row = framebuf_height_chars();
        g_max_col = framebuf_width_chars();

        g_output_impl.p_init = framebuf_init;
        g_output_impl.p_map_iomem = framebuf_map_iomem;

        g_output_impl.p_put_char_at = framebuf_put_char_at;
        g_output_impl.p_put_cursor_at = framebuf_put_cursor_at;
        g_output_impl.p_clear_rows = framebuf_clear_rows;
        g_output_impl.p_scroll_new_row = framebuf_scroll_new_row;
    } else {
        LOG_DEBUG("terminal type - VGA text mode");

        vga_early_init();

        g_max_row = vga_height_chars();
        g_max_col = vga_width_chars();

        g_output_impl.p_init = NULL;
        g_output_impl.p_map_iomem = vga_map_iomem;

        g_output_impl.p_put_char_at = vga_put_char_at;
        g_output_impl.p_put_cursor_at = vga_put_cursor_at;
        g_output_impl.p_clear_rows = vga_clear_rows;
        g_output_impl.p_scroll_new_row = vga_scroll_new_row;
    }

    g_term_has_impl = true;

    mutex_init(&g_term_mutex);
}

void term_init(void) {
    if (g_output_impl.p_init) { g_output_impl.p_init(); }
}

void term_map_iomem(void) {
    if (g_output_impl.p_map_iomem) { g_output_impl.p_map_iomem(); }
}

[[gnu::noreturn]]
void term_task(void) {
    kbd_event_t event;
    for (;;) {
        queue_read(kbd_sysevent_queue(), &event, sizeof(kbd_event_t));
    }
}

void term_acquire_mutex(void) {
    if (!gb_panic_mode) { mutex_acquire(&g_term_mutex); }
}

void term_release_mutex(void) {
    if (!gb_panic_mode) { mutex_release(&g_term_mutex); }
}

bool term_owns_mutex(void) {
    return mutex_caller_owns(&g_term_mutex);
}

void term_enter_panic_mode(void) {
    gb_panic_mode = true;
}

void term_clear(void) {
    if (!g_term_has_impl) { return; }

    assert_owns_mutex();
    g_output_impl.p_clear_rows(0, g_max_row);
    put_cursor_at(0, 0);
}

void term_clear_rows(size_t start_row, size_t num_rows) {
    if (!g_term_has_impl) { return; }

    assert_owns_mutex();
    g_output_impl.p_clear_rows(start_row, num_rows);
}

void term_print_str(char const *p_str) {
    if (!g_term_has_impl) { return; }

    assert_owns_mutex();
    while ((*p_str) != 0) {
        char ch = (*p_str);
        put_char(ch);
        p_str++;
    }
    term_put_cursor_at(g_row, g_col);
}

void term_print_str_len(char const *p_str, size_t len) {
    if (!g_term_has_impl) { return; }

    assert_owns_mutex();
    for (size_t idx = 0; idx < len; idx++) {
        put_char(p_str[idx]);
    }
    put_cursor_at(g_row, g_col);
}

void term_put_char_at(size_t row, size_t col, char ch) {
    if (!g_term_has_impl) { return; }

    assert_owns_mutex();
    g_output_impl.p_put_char_at(row, col, ch);
}

void term_put_cursor_at(size_t row, size_t col) {
    if (!g_term_has_impl) { return; }

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
    kbd_event_t event;
    queue_read(kbd_event_queue(), &event, sizeof(kbd_event_t));
    *p_event = event;
}

static void put_char(char ch) {
    switch (ch) {
    case '\n':
        g_col = 0;
        g_row++;
        if (g_row >= g_max_row) {
            g_row = (g_max_row - 1);
            scroll_new_row();
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
                scroll_new_row();
            }
        }
    }
}

static void scroll_new_row(void) {
    g_output_impl.p_scroll_new_row();
}
