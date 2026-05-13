#include "framebuf.h"
#include "kbd.h"
#include "kmutex.h"
#include "log.h"
#include "mbi.h"
#include "panic.h"
#include "queue.h"
#include "textdisp.h"
#include "vga.h"

typedef struct {
    void (*p_init)(void);
    void (*p_map_iomem)(void);

    void (*p_put_char_at)(size_t row, size_t col, char ch);
    void (*p_put_cursor_at)(size_t row, size_t col);
    void (*p_clear_rows)(size_t start_row, size_t num_rows);
    void (*p_scroll_new_row)(void);
} output_impl_t;

typedef struct {
    /**
     * Text display struct lock.
     *
     * To maintain coherent output on the display, each task that wants to print
     * must lock this mutex. See #textdisp_lock(), #textdisp_unlock(),
     * #textdisp_owns_lock().
     */
    task_mutex_t lock;

    bool panic_mode;
    output_impl_t impl;
    bool has_impl;

    size_t max_row;
    size_t max_col;

    size_t row;
    size_t col;
} textdisp_t;

/**
 * Boot display context.
 *
 * The "boot text display" is whichever display has its framebuffer passed by
 * the bootloader (in case of @link framebuf.c @endlink) or whichever display is
 * mapped at #VGA_MMIO_START.
 */
static textdisp_t g_textdisp;

static void put_char(char ch);
static void scroll_new_row(void);

[[gnu::artificial]]
static inline void assert_owns_mutex(void) {
    if (!mutex_caller_owns(&g_textdisp.lock) && !g_textdisp.panic_mode) {
        panic_nested();
    }
}

[[gnu::artificial]]
static inline void put_cursor_at(size_t row, size_t col) {
    g_textdisp.impl.p_put_cursor_at(row, col);
    g_textdisp.row = row;
    g_textdisp.col = col;
}

void textdisp_early_init(void) {
    mbi_t const *p_mbi = mbi_ptr();

    if ((p_mbi->flags & MBI_FLAG_FRAMEBUF) &&
        (MBI_FRAMEBUF_EGA != p_mbi->framebuffer_type)) {
        LOG_DEBUG("terminal type - framebuffer");

        framebuf_early_init();

        g_textdisp.max_row = framebuf_height_chars();
        g_textdisp.max_col = framebuf_width_chars();

        g_textdisp.impl.p_init = framebuf_init;
        g_textdisp.impl.p_map_iomem = framebuf_map_iomem;

        g_textdisp.impl.p_put_char_at = framebuf_put_char_at;
        g_textdisp.impl.p_put_cursor_at = framebuf_put_cursor_at;
        g_textdisp.impl.p_clear_rows = framebuf_clear_rows;
        g_textdisp.impl.p_scroll_new_row = framebuf_scroll_new_row;
    } else {
        LOG_DEBUG("terminal type - VGA text mode");

        vga_early_init();

        g_textdisp.max_row = vga_height_chars();
        g_textdisp.max_col = vga_width_chars();

        g_textdisp.impl.p_init = NULL;
        g_textdisp.impl.p_map_iomem = vga_map_iomem;

        g_textdisp.impl.p_put_char_at = vga_put_char_at;
        g_textdisp.impl.p_put_cursor_at = vga_put_cursor_at;
        g_textdisp.impl.p_clear_rows = vga_clear_rows;
        g_textdisp.impl.p_scroll_new_row = vga_scroll_new_row;
    }

    g_textdisp.has_impl = true;

    mutex_init(&g_textdisp.lock);
}

void textdisp_init(void) {
    if (g_textdisp.impl.p_init) { g_textdisp.impl.p_init(); }
}

void textdisp_map_iomem(void) {
    if (g_textdisp.impl.p_map_iomem) { g_textdisp.impl.p_map_iomem(); }
}

[[gnu::noreturn]]
void textdisp_task(void) {
    kbd_event_t event;
    for (;;) {
        queue_read(kbd_sysevent_queue(), &event, sizeof(kbd_event_t));
    }
}

void textdisp_lock(void) {
    if (!g_textdisp.panic_mode) { mutex_acquire(&g_textdisp.lock); }
}

void textdisp_unlock(void) {
    if (!g_textdisp.panic_mode) { mutex_release(&g_textdisp.lock); }
}

bool textdisp_owns_lock(void) {
    return mutex_caller_owns(&g_textdisp.lock);
}

void textdisp_begin_panic(void) {
    g_textdisp.panic_mode = true;
}

void textdisp_clear(void) {
    if (!g_textdisp.has_impl) { return; }

    assert_owns_mutex();
    g_textdisp.impl.p_clear_rows(0, g_textdisp.max_row);
    put_cursor_at(0, 0);
}

void textdisp_clear_rows(size_t start_row, size_t num_rows) {
    if (!g_textdisp.has_impl) { return; }

    assert_owns_mutex();
    g_textdisp.impl.p_clear_rows(start_row, num_rows);
}

void textdisp_print_str(char const *p_str) {
    if (!g_textdisp.has_impl) { return; }

    assert_owns_mutex();
    while ((*p_str) != 0) {
        char ch = (*p_str);
        put_char(ch);
        p_str++;
    }
    textdisp_put_cursor_at(g_textdisp.row, g_textdisp.col);
}

void textdisp_print_str_len(char const *p_str, size_t len) {
    if (!g_textdisp.has_impl) { return; }

    assert_owns_mutex();
    for (size_t idx = 0; idx < len; idx++) {
        put_char(p_str[idx]);
    }
    put_cursor_at(g_textdisp.row, g_textdisp.col);
}

void textdisp_put_char_at(size_t row, size_t col, char ch) {
    if (!g_textdisp.has_impl) { return; }

    assert_owns_mutex();
    g_textdisp.impl.p_put_char_at(row, col, ch);
}

void textdisp_put_cursor_at(size_t row, size_t col) {
    if (!g_textdisp.has_impl) { return; }

    assert_owns_mutex();
    put_cursor_at(row, col);
}

size_t textdisp_row(void) {
    return g_textdisp.row;
}

size_t textdisp_col(void) {
    return g_textdisp.col;
}

size_t textdisp_height(void) {
    return g_textdisp.max_row;
}

size_t textdisp_width(void) {
    return g_textdisp.max_col;
}

void textdisp_read_kbd_event(kbd_event_t *p_event) {
    kbd_event_t event;
    queue_read(kbd_event_queue(), &event, sizeof(kbd_event_t));
    *p_event = event;
}

static void put_char(char ch) {
    switch (ch) {
    case '\n':
        g_textdisp.col = 0;
        g_textdisp.row++;
        if (g_textdisp.row >= g_textdisp.max_row) {
            g_textdisp.row = (g_textdisp.max_row - 1);
            scroll_new_row();
        }
        break;

    default:
        g_textdisp.impl.p_put_char_at(g_textdisp.row, g_textdisp.col, ch);

        g_textdisp.col++;
        if (g_textdisp.col >= g_textdisp.max_col) {
            g_textdisp.col = 0;
            g_textdisp.row++;
            if (g_textdisp.row >= g_textdisp.max_row) {
                g_textdisp.row = (g_textdisp.max_row - 1);
                scroll_new_row();
            }
        }
    }
}

static void scroll_new_row(void) {
    g_textdisp.impl.p_scroll_new_row();
}
