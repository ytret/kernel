#include "framebuf.h"
#include "kbd.h"
#include "kmutex.h"
#include "log.h"
#include "mbi.h"
#include "panic.h"
#include "queue.h"
#include "textdisp.h"
#include "vga.h"

struct textdisp {
    /**
     * Text display struct lock.
     *
     * To maintain coherent output on the display, each task that wants to print
     * must lock this mutex. See #textdisp_lock(), #textdisp_unlock(),
     * #textdisp_owns_lock().
     */
    task_mutex_t lock;

    bool panic_mode;
    const textdisp_ops_t *ops;
    bool has_impl;

    size_t max_row;
    size_t max_col;

    size_t row;
    size_t col;
};

/**
 * Boot display context.
 *
 * The "boot text display" is whichever display has its framebuffer passed by
 * the bootloader (in case of @link framebuf.c @endlink) or whichever display is
 * mapped at #VGA_MMIO_START.
 */
static textdisp_t g_textdisp;

static void put_char(textdisp_t *disp, char ch);
static void scroll_new_row(textdisp_t *disp);

[[gnu::artificial]]
static inline void assert_owns_mutex(textdisp_t *disp) {
    if (!mutex_caller_owns(&disp->lock) && !disp->panic_mode) {
        panic_nested();
    }
}

[[gnu::artificial]]
static inline void put_cursor_at(textdisp_t *disp, size_t row, size_t col) {
    disp->ops->p_put_cursor_at(row, col);
    disp->row = row;
    disp->col = col;
}

textdisp_t *textdisp_get_boot_disp(void) {
    return &g_textdisp;
}

void textdisp_early_init(textdisp_t *disp) {
    mbi_t const *p_mbi = mbi_ptr();

    if ((p_mbi->flags & MBI_FLAG_FRAMEBUF) &&
        (MBI_FRAMEBUF_EGA != p_mbi->framebuffer_type)) {
        LOG_DEBUG("terminal type - framebuffer");

        framebuf_early_init();
        disp->max_row = framebuf_height_chars();
        disp->max_col = framebuf_width_chars();
        disp->ops = framebuf_textdisp_ops();
    } else {
        LOG_DEBUG("terminal type - VGA text mode");

        vga_early_init();
        disp->max_row = vga_height_chars();
        disp->max_col = vga_width_chars();
        disp->ops = vga_textdisp_ops();
    }

    disp->has_impl = true;

    mutex_init(&disp->lock);
}

void textdisp_init(textdisp_t *disp) {
    if (disp->ops->p_init) { disp->ops->p_init(); }
}

void textdisp_map_iomem(textdisp_t *disp) {
    if (disp->ops->p_map_iomem) { disp->ops->p_map_iomem(); }
}

[[gnu::noreturn]]
void textdisp_task(void) {
    kbd_event_t event;
    for (;;) {
        queue_read(kbd_sysevent_queue(), &event, sizeof(kbd_event_t));
    }
}

void textdisp_lock(textdisp_t *disp) {
    if (!disp->panic_mode) { mutex_acquire(&disp->lock); }
}

void textdisp_unlock(textdisp_t *disp) {
    if (!disp->panic_mode) { mutex_release(&disp->lock); }
}

bool textdisp_owns_lock(textdisp_t *disp) {
    return mutex_caller_owns(&disp->lock);
}

void textdisp_begin_panic(textdisp_t *disp) {
    disp->panic_mode = true;
}

void textdisp_clear(textdisp_t *disp) {
    if (!disp->has_impl) { return; }

    assert_owns_mutex(disp);
    disp->ops->p_clear_rows(0, disp->max_row);
    put_cursor_at(disp, 0, 0);
}

void textdisp_clear_rows(textdisp_t *disp, size_t start_row, size_t num_rows) {
    if (!disp->has_impl) { return; }

    assert_owns_mutex(disp);
    disp->ops->p_clear_rows(start_row, num_rows);
}

void textdisp_print_str(textdisp_t *disp, char const *p_str) {
    if (!disp->has_impl) { return; }

    assert_owns_mutex(disp);
    while ((*p_str) != 0) {
        char ch = (*p_str);
        put_char(disp, ch);
        p_str++;
    }
    textdisp_put_cursor_at(disp, disp->row, disp->col);
}

void textdisp_print_str_len(textdisp_t *disp, char const *p_str, size_t len) {
    if (!disp->has_impl) { return; }

    assert_owns_mutex(disp);
    for (size_t idx = 0; idx < len; idx++) {
        put_char(disp, p_str[idx]);
    }
    put_cursor_at(disp, disp->row, disp->col);
}

void textdisp_put_char_at(textdisp_t *disp, size_t row, size_t col, char ch) {
    if (!disp->has_impl) { return; }

    assert_owns_mutex(disp);
    disp->ops->p_put_char_at(row, col, ch);
}

void textdisp_put_cursor_at(textdisp_t *disp, size_t row, size_t col) {
    if (!disp->has_impl) { return; }

    assert_owns_mutex(disp);
    put_cursor_at(disp, row, col);
}

size_t textdisp_row(textdisp_t *disp) {
    return disp->row;
}

size_t textdisp_col(textdisp_t *disp) {
    return disp->col;
}

size_t textdisp_height(textdisp_t *disp) {
    return disp->max_row;
}

size_t textdisp_width(textdisp_t *disp) {
    return disp->max_col;
}

void textdisp_read_kbd_event(kbd_event_t *p_event) {
    kbd_event_t event;
    queue_read(kbd_event_queue(), &event, sizeof(kbd_event_t));
    *p_event = event;
}

static void put_char(textdisp_t *disp, char ch) {
    switch (ch) {
    case '\n':
        disp->col = 0;
        disp->row++;
        if (disp->row >= disp->max_row) {
            disp->row = (disp->max_row - 1);
            scroll_new_row(disp);
        }
        break;

    default:
        disp->ops->p_put_char_at(disp->row, disp->col, ch);

        disp->col++;
        if (disp->col >= disp->max_col) {
            disp->col = 0;
            disp->row++;
            if (disp->row >= disp->max_row) {
                disp->row = (disp->max_row - 1);
                scroll_new_row(disp);
            }
        }
    }
}

static void scroll_new_row(textdisp_t *disp) {
    disp->ops->p_scroll_new_row();
}
