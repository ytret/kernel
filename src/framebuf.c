/*
 * Framebuffer terminal implementation.
 *
 * For comments and explanation on the 'shadow buffer' which is used for saving
 * output history, see vga.c.
 */

#include "assert.h"
#include "framebuf.h"
#include "heap.h"
#include "mbi.h"
#include "memfun.h"
#include "psf.h"

#define SHADOW_SCREENS 2

static uint8_t *gp_framebuf;
static psf_t g_font;

static size_t g_height_px;
static size_t g_height_chars;
static size_t g_width_px;
static size_t g_width_chars;

static uint32_t g_px_pitch;
static uint32_t g_row_pitch;
static uint8_t g_bpp;

static uint8_t *gp_shadow_buf;
static size_t g_fb_start_at_sh_row;

static size_t g_cursor_lss_row;
static size_t g_cursor_lss_col;

static bool get_fb_idx(size_t sh_row, size_t sh_col, size_t *p_fb_row,
                       size_t *p_fb_col);
static void get_fb_row_range(size_t lss_start_row, size_t lss_num_rows,
                             size_t *p_fb_start_row, size_t *p_fb_num_rows);

static void draw_glyph_sh(size_t sh_row, size_t sh_col, char ch);
static void draw_glyph_fb(size_t fb_row, size_t fb_col, char ch);
static void draw_glyph(uint8_t *p_buf, size_t y, size_t x, char ch);
static void draw_pixel(uint8_t *p_buf, size_t y, size_t x);
static void clear_pixel(uint8_t *p_buf, size_t y, size_t x);

static void enable_cursor(void);
static void disable_cursor(void);
static void draw_cursor_fb(void);

static void copy_shadow_to_fb(void);

void framebuf_init(void) {
    const mbi_t *p_mbi = mbi_ptr();

    gp_framebuf = (uint8_t *)((uint32_t)p_mbi->framebuffer_addr);
    g_height_px = p_mbi->framebuffer_height;
    g_width_px = p_mbi->framebuffer_width;
    g_px_pitch = p_mbi->framebuffer_pitch;
    g_bpp = p_mbi->framebuffer_bpp;

    const mbi_mod_t *p_mod = mbi_find_mod("font");
    ASSERT(p_mod);
    bool b_psf_loaded = psf_load(&g_font, p_mod->mod_start);
    ASSERT(b_psf_loaded);

    g_height_chars = g_height_px / g_font.height_px;
    g_width_chars = g_width_px / g_font.width_px;
    g_row_pitch = g_px_pitch * g_font.height_px;
}

uint32_t framebuf_start(void) {
    return (uint32_t)gp_framebuf;
}

uint32_t framebuf_end(void) {
    uint32_t fb_start = framebuf_start();
    return fb_start + g_height_px * g_px_pitch;
}

size_t framebuf_height_chars(void) {
    return g_height_chars;
}

size_t framebuf_width_chars(void) {
    return g_width_chars;
}

/*
 * Places a character on the last shadow screen.
 */
void framebuf_put_char_at(size_t lss_row, size_t lss_col, char ch) {
    if (!gp_shadow_buf) { return; }

    ASSERT(lss_row < g_height_chars && lss_col < g_width_chars);

    size_t sh_row = (SHADOW_SCREENS - 1) * g_height_chars + lss_row;
    size_t sh_col = lss_col;
    draw_glyph_sh(sh_row, sh_col, ch);

    size_t fb_row;
    size_t fb_col;
    bool b_visible = get_fb_idx(sh_row, sh_col, &fb_row, &fb_col);
    if (b_visible) { draw_glyph_fb(fb_row, fb_col, ch); }
}

/*
 * Places a new cursor on the last shadow screen, without deleting the previous
 * one.
 */
void framebuf_put_cursor_at(size_t lss_row, size_t lss_col) {
    if (!gp_shadow_buf) { return; }

    g_cursor_lss_row = lss_row;
    g_cursor_lss_col = lss_col;
    draw_cursor_fb();
}

/*
 * Clears rows on the last shadow screen.
 */
void framebuf_clear_rows(size_t lss_start_row, size_t lss_num_rows) {
    if (!gp_shadow_buf) { return; }

    size_t sh_start_row = (SHADOW_SCREENS - 1) * g_height_chars + lss_start_row;
    size_t sh_num_bytes = lss_num_rows * g_row_pitch;
    memclr_sse2(&gp_shadow_buf[sh_start_row * g_row_pitch], sh_num_bytes);

    size_t fb_start_row;
    size_t fb_num_rows;
    get_fb_row_range(lss_start_row, lss_num_rows, &fb_start_row, &fb_num_rows);
    memclr_sse2(&gp_framebuf[fb_start_row * g_row_pitch],
           fb_num_rows * g_row_pitch);
}

/*
 * Scrolls the shadow buffer so that one empty row is available at the bottom.
 */
void framebuf_scroll_new_row(void) {
    if (!gp_shadow_buf) { return; }

    // Move every shadow row except the first one up.
    memmove_sse2(gp_shadow_buf, &gp_shadow_buf[1 * g_row_pitch],
                 (SHADOW_SCREENS * g_height_chars - 1) * g_row_pitch);

    // Clear the last shadow row.
    size_t sh_last_row = SHADOW_SCREENS * g_height_chars - 1;
    memclr_sse2(&gp_shadow_buf[sh_last_row * g_row_pitch], g_row_pitch);

    copy_shadow_to_fb();
}

void framebuf_init_history(void) {
    gp_shadow_buf =
        heap_alloc_aligned(g_height_px * g_px_pitch * SHADOW_SCREENS, 16);
    g_fb_start_at_sh_row = (SHADOW_SCREENS - 1) * g_height_chars;
}

/*
 * Resets the shadow buffer thus deleting all output history. Also deletes the
 * currently visible part of it WITHOUT updating the framebuffer.
 */
void framebuf_clear_history(void) {
    if (!gp_shadow_buf) { return; }

    memclr_sse2(gp_shadow_buf, g_height_px * g_px_pitch * SHADOW_SCREENS);
    g_fb_start_at_sh_row = (SHADOW_SCREENS - 1) * g_height_chars;
}

size_t framebuf_history_screens(void) {
    return SHADOW_SCREENS;
}

/*
 * Returns the row number in the shadow buffer of the first visible row.
 */
size_t framebuf_history_pos(void) {
    return g_fb_start_at_sh_row;
}

void framebuf_set_history_mode(size_t start_at_sh_row) {
    if (!gp_shadow_buf) { return; }

    ASSERT(start_at_sh_row <= (SHADOW_SCREENS - 1) * g_height_chars);

    g_fb_start_at_sh_row = start_at_sh_row;
    copy_shadow_to_fb();

    if (g_fb_start_at_sh_row < (SHADOW_SCREENS - 1) * g_height_chars) {
        disable_cursor();
    } else {
        enable_cursor();
    }
}

bool framebuf_is_history_mode_active(void) {
    if (!gp_shadow_buf) { return false; }

    return g_fb_start_at_sh_row < (SHADOW_SCREENS - 1) * g_height_chars;
}

/*
 * Converts the position on the last shadow buffer screen to a framebuffer
 * offset, if the position is visible.
 *
 * Returns true if the position is visible and *p_fb_row and *p_fb_col have been
 * written.
 */
static bool get_fb_idx(size_t sh_row, size_t sh_col, size_t *p_fb_row,
                       size_t *p_fb_col) {
    if (sh_row >= g_fb_start_at_sh_row &&
        sh_row < g_fb_start_at_sh_row + g_height_chars) {
        *p_fb_row = sh_row - g_fb_start_at_sh_row;
        *p_fb_col = sh_col;
        return true;
    } else {
        return false;
    }
}

/*
 * Similar to get_fb_idx, but converts row ranges on the last shadow screen to
 * their visible counterpart.
 */
static void get_fb_row_range(size_t lss_start_row, size_t lss_num_rows,
                             size_t *p_fb_start_row, size_t *p_fb_num_rows) {
    size_t sh_start_row = (SHADOW_SCREENS - 1) * g_height_chars + lss_start_row;
    size_t sh_end_row = sh_start_row + lss_num_rows;

    if (sh_start_row >= g_fb_start_at_sh_row) {
        *p_fb_start_row = sh_start_row - g_fb_start_at_sh_row;

        if (sh_end_row > g_fb_start_at_sh_row + g_height_chars) {
            *p_fb_num_rows =
                lss_num_rows -
                (sh_end_row - (g_fb_start_at_sh_row + g_height_chars));
        } else {
            *p_fb_num_rows = lss_num_rows;
        }
    } else {
        *p_fb_start_row = 0;
        *p_fb_num_rows = 0;
    }
}

/*
 * Draws a glyph in the shadow buffer WITHOUT updating the framebuffer even if
 * the position is visible.
 */
static void draw_glyph_sh(size_t sh_row, size_t sh_col, char ch) {
    size_t sh_y = sh_row * g_font.height_px;
    size_t sh_x = sh_col * g_font.width_px;
    draw_glyph(gp_shadow_buf, sh_y, sh_x, ch);
}

static void draw_glyph_fb(size_t fb_row, size_t fb_col, char ch) {
    size_t fb_y = fb_row * g_font.height_px;
    size_t fb_x = fb_col * g_font.width_px;
    draw_glyph(gp_framebuf, fb_y, fb_x, ch);
}

static void draw_glyph(uint8_t *p_buf, size_t y, size_t x, char ch) {
    const uint8_t *p_glyph = psf_glyph(&g_font, ch);
    for (size_t it_y = 0; it_y < g_font.height_px; it_y++) {
        for (size_t it_x = 0; it_x < g_font.width_px; it_x++) {
            uint8_t val = p_glyph[it_y] & (1 << (7 - it_x));
            if (val) {
                draw_pixel(p_buf, y + it_y, x + it_x);
            } else {
                clear_pixel(p_buf, y + it_y, x + it_x);
            }
        }
    }
}

static void draw_pixel(uint8_t *p_buf, size_t y, size_t x) {
    size_t offset = y * g_px_pitch + x * (g_bpp / 8);
    p_buf[offset + 0] = 0xFF;
    p_buf[offset + 1] = 0xFF;
    p_buf[offset + 2] = 0xFF;
}

static void clear_pixel(uint8_t *p_buf, size_t y, size_t x) {
    size_t offset = y * g_px_pitch + x * (g_bpp / 8);
    memset(&p_buf[offset], 0, g_bpp / 8);
}

static void enable_cursor(void) {
}

static void disable_cursor(void) {
}

static void draw_cursor_fb(void) {
    size_t sh_row = (SHADOW_SCREENS - 1) * g_height_chars + g_cursor_lss_row;
    size_t sh_col = g_cursor_lss_col;

    size_t fb_row;
    size_t fb_col;
    bool b_visible = get_fb_idx(sh_row, sh_col, &fb_row, &fb_col);
    if (b_visible) {
        // TODO:
    }
}

static void copy_shadow_to_fb(void) {
    memmove_sse2(gp_framebuf,
                 &gp_shadow_buf[g_fb_start_at_sh_row * g_row_pitch],
                 g_height_px * g_px_pitch);
    draw_cursor_fb();
}
