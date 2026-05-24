#include "textdisp.h"

/**
 * Boot display context.
 *
 * The "boot text display" is whichever display has its framebuffer passed by
 * the bootloader (in case of @link framebuf.c @endlink) or whichever display is
 * mapped at #VGA_MMIO_START.
 */
static textdisp_t g_textdisp;

[[gnu::artificial]]
static inline void put_cursor_at(textdisp_t *disp, size_t row, size_t col) {
    disp->ops->p_put_cursor_at(row, col);
}

textdisp_t *textdisp_get_boot_disp(void) {
    return &g_textdisp;
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

void textdisp_clear(textdisp_t *disp) {
    if (!disp->has_impl) { return; }

    disp->ops->p_clear_rows(0, disp->max_row);
    put_cursor_at(disp, 0, 0);
}

void textdisp_clear_rows(textdisp_t *disp, size_t start_row, size_t num_rows) {
    if (!disp->has_impl) { return; }

    disp->ops->p_clear_rows(start_row, num_rows);
}

void textdisp_put_char_at(textdisp_t *disp, size_t row, size_t col, char ch) {
    if (!disp->has_impl) { return; }

    disp->ops->p_put_char_at(row, col, ch);
}

void textdisp_put_cursor_at(textdisp_t *disp, size_t row, size_t col) {
    if (!disp->has_impl) { return; }

    put_cursor_at(disp, row, col);
}

void textdisp_scroll(textdisp_t *disp) {
    if (!disp->has_impl) { return; }

    disp->ops->p_scroll_new_row();
}
