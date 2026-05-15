#include "assert.h"
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
     * must lock this mutex.
     *
     * See #textdisp.lock_cnt.
     */
    task_mutex_t lock;

    /**
     * Nested lock counter for #textdisp.lock.
     *
     * See #textdisp_lock() and #textdisp_unlock().
     */
    int lock_cnt;

    const textdisp_ops_t *ops;
    bool has_impl;

    size_t max_row;
    size_t max_col;
};

/**
 * Boot display context.
 *
 * The "boot text display" is whichever display has its framebuffer passed by
 * the bootloader (in case of @link framebuf.c @endlink) or whichever display is
 * mapped at #VGA_MMIO_START.
 */
static textdisp_t g_textdisp;

[[gnu::artificial]]
static inline void assert_owns_mutex(textdisp_t *disp) {
    if (!mutex_caller_owns(&disp->lock)) { panic_nested(); }
}

[[gnu::artificial]]
static inline void put_cursor_at(textdisp_t *disp, size_t row, size_t col) {
    disp->ops->p_put_cursor_at(row, col);
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

size_t textdisp_height(textdisp_t *disp) {
    return disp->max_row;
}

size_t textdisp_width(textdisp_t *disp) {
    return disp->max_col;
}

void textdisp_lock(textdisp_t *disp) {
    if (!mutex_caller_owns(&disp->lock)) { mutex_acquire(&disp->lock); }
    disp->lock_cnt++;
}

void textdisp_unlock(textdisp_t *disp) {
    ASSERT(disp->lock_cnt > 0);
    disp->lock_cnt--;
    if (disp->lock_cnt == 0) { mutex_release(&disp->lock); }
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

void textdisp_scroll(textdisp_t *disp) {
    if (!disp->has_impl) { return; }

    assert_owns_mutex(disp);
    disp->ops->p_scroll_new_row();
}

void textdisp_read_kbd_event(kbd_event_t *p_event) {
    kbd_event_t event;
    queue_read(kbd_event_queue(), &event, sizeof(kbd_event_t));
    *p_event = event;
}
