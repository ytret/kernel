/**
 * @file framebuf.c
 * Framebuffer terminal implementation.
 *
 * For the explanation of the 'shadow buffer' which is used for saving terminal
 * history, see vga.c.
 */

#include "assert.h"
#include "framebuf.h"
#include "heap.h"
#include "kinttypes.h"
#include "log.h"
#include "mbi.h"
#include "memfun.h"
#include "psf.h"
#include "vmm.h"

/**
 * Size of the shadow buffer in screens.
 */
#define FRAMEBUF_NUM_SHADOW_SCREENS 2

static uint8_t *gp_framebuf;
static psf_t g_fb_font;

static size_t g_fb_height_px;
static size_t g_fb_height_ch;
static size_t g_fb_width_px;
static size_t g_fb_width_ch;

static uint32_t g_fb_pitch_px;
static uint32_t g_fb_pitch_row;
static uint8_t g_fb_bpp;

static uint8_t *g_fb_shadow_buf;

/**
 * First row in the shadow buffer that backs the first framebuffer row.
 *
 * Everything starting from this row in the shadow buffer (and ending
 * #g_fb_height_ch rows later) is a copy of what is currently in the
 * framebuffer.
 */
static size_t g_fb_start_at_sh_row;

static bool g_fb_cursor_en = true;
static size_t g_fb_cursor_lss_row;
static size_t g_fb_cursor_lss_col;

static bool prv_fb_get_buf_idx(size_t sh_row, size_t sh_col, size_t *p_fb_row,
                               size_t *p_fb_col);
static void prv_fb_get_buf_range(size_t lss_start_row, size_t lss_num_rows,
                                 size_t *p_fb_start_row, size_t *p_fb_num_rows);

static void prv_fb_draw_sh_glyph(size_t sh_row, size_t sh_col, char ch);
static void prv_fb_draw_real_glyph(size_t fb_row, size_t fb_col, char ch);
static void prv_fb_draw_glyph_in(uint8_t *p_buf, size_t y, size_t x, char ch);
static void prv_fb_draw_pixel(uint8_t *p_buf, size_t y, size_t x);
static void prv_fb_clear_pixel(uint8_t *p_buf, size_t y, size_t x);
static void prv_fb_invert_pixel(uint8_t *p_buf, size_t y, size_t x);

static void prv_fb_enable_cursor(void);
static void prv_fb_disable_cursor(void);
static void prv_fb_draw_real_cursor(void);

static void prv_fb_copy_sh_to_real(size_t fb_start_row, size_t fb_num_rows);

void framebuf_init(void) {
    const mbi_t *p_mbi = mbi_ptr();

    gp_framebuf = (uint8_t *)((uint32_t)p_mbi->framebuffer_addr);
    g_fb_height_px = p_mbi->framebuffer_height;
    g_fb_width_px = p_mbi->framebuffer_width;
    g_fb_pitch_px = p_mbi->framebuffer_pitch;
    g_fb_bpp = p_mbi->framebuffer_bpp;

    const mbi_mod_t *p_mod = mbi_find_mod("font");
    ASSERT(p_mod);
    bool b_psf_loaded = psf_load(&g_fb_font, p_mod->mod_start);
    ASSERT(b_psf_loaded);

    g_fb_height_ch = g_fb_height_px / g_fb_font.height_px;
    g_fb_width_ch = g_fb_width_px / g_fb_font.width_px;
    g_fb_pitch_row = g_fb_pitch_px * g_fb_font.height_px;
}

void framebuf_map_iomem(void) {
    const uint32_t iomem_start = framebuf_start();
    const uint32_t iomem_end = framebuf_end();

    LOG_DEBUG("map framebuffer memory 0x%08" PRIx32 " .. 0x%08" PRIx32,
              iomem_start, iomem_end);
    for (uint32_t page = iomem_start; page < iomem_end; page += 4096) {
        vmm_map_kernel_page(page, page);
    }
}

uint32_t framebuf_start(void) {
    return (uint32_t)gp_framebuf;
}

uint32_t framebuf_end(void) {
    uint32_t fb_start = framebuf_start();
    return fb_start + g_fb_height_px * g_fb_pitch_px;
}

size_t framebuf_height_chars(void) {
    return g_fb_height_ch;
}

size_t framebuf_width_chars(void) {
    return g_fb_width_ch;
}

void framebuf_put_char_at(size_t lss_row, size_t lss_col, char ch) {
    if (!g_fb_shadow_buf) { return; }

    ASSERT(lss_row < g_fb_height_ch && lss_col < g_fb_width_ch);

    size_t sh_row =
        (FRAMEBUF_NUM_SHADOW_SCREENS - 1) * g_fb_height_ch + lss_row;
    size_t sh_col = lss_col;
    prv_fb_draw_sh_glyph(sh_row, sh_col, ch);

    size_t fb_row;
    size_t fb_col;
    bool b_visible = prv_fb_get_buf_idx(sh_row, sh_col, &fb_row, &fb_col);
    if (b_visible) { prv_fb_draw_real_glyph(fb_row, fb_col, ch); }
}

void framebuf_put_cursor_at(size_t lss_row, size_t lss_col) {
    if (!g_fb_shadow_buf) { return; }

    // Remove the previous cursor.
    prv_fb_copy_sh_to_real(g_fb_cursor_lss_row, 1);

    g_fb_cursor_lss_row = lss_row;
    g_fb_cursor_lss_col = lss_col;

    if (g_fb_cursor_en) { prv_fb_draw_real_cursor(); }
}

void framebuf_clear_rows(size_t lss_start_row, size_t lss_num_rows) {
    if (!g_fb_shadow_buf) { return; }

    size_t sh_start_row =
        (FRAMEBUF_NUM_SHADOW_SCREENS - 1) * g_fb_height_ch + lss_start_row;
    size_t sh_num_bytes = lss_num_rows * g_fb_pitch_row;
    kmemclr_sse2(&g_fb_shadow_buf[sh_start_row * g_fb_pitch_row], sh_num_bytes);

    size_t fb_start_row;
    size_t fb_num_rows;
    prv_fb_get_buf_range(lss_start_row, lss_num_rows, &fb_start_row,
                         &fb_num_rows);
    kmemclr_sse2(&gp_framebuf[fb_start_row * g_fb_pitch_row],
                 fb_num_rows * g_fb_pitch_row);
}

void framebuf_scroll_new_row(void) {
    if (!g_fb_shadow_buf) { return; }

    // Move every shadow row except the first one up.
    kmemmove_sse2(g_fb_shadow_buf, &g_fb_shadow_buf[1 * g_fb_pitch_row],
                  (FRAMEBUF_NUM_SHADOW_SCREENS * g_fb_height_ch - 1) *
                      g_fb_pitch_row);

    // Clear the last shadow row.
    size_t sh_last_row = FRAMEBUF_NUM_SHADOW_SCREENS * g_fb_height_ch - 1;
    kmemclr_sse2(&g_fb_shadow_buf[sh_last_row * g_fb_pitch_row],
                 g_fb_pitch_row);

    prv_fb_copy_sh_to_real(0, g_fb_height_ch);
}

void framebuf_init_history(void) {
    g_fb_shadow_buf = heap_alloc_aligned(
        g_fb_height_px * g_fb_pitch_px * FRAMEBUF_NUM_SHADOW_SCREENS, 16);
    g_fb_start_at_sh_row = (FRAMEBUF_NUM_SHADOW_SCREENS - 1) * g_fb_height_ch;
}

void framebuf_clear_history(void) {
    if (!g_fb_shadow_buf) { return; }

    kmemclr_sse2(g_fb_shadow_buf,
                 g_fb_height_px * g_fb_pitch_px * FRAMEBUF_NUM_SHADOW_SCREENS);
    g_fb_start_at_sh_row = (FRAMEBUF_NUM_SHADOW_SCREENS - 1) * g_fb_height_ch;
}

size_t framebuf_history_screens(void) {
    return FRAMEBUF_NUM_SHADOW_SCREENS;
}

size_t framebuf_history_pos(void) {
    return g_fb_start_at_sh_row;
}

void framebuf_set_history_mode(size_t start_at_sh_row) {
    if (!g_fb_shadow_buf) { return; }

    ASSERT(start_at_sh_row <=
           (FRAMEBUF_NUM_SHADOW_SCREENS - 1) * g_fb_height_ch);

    g_fb_start_at_sh_row = start_at_sh_row;
    prv_fb_copy_sh_to_real(0, g_fb_height_ch);

    if (g_fb_start_at_sh_row <
        (FRAMEBUF_NUM_SHADOW_SCREENS - 1) * g_fb_height_ch) {
        prv_fb_disable_cursor();
    } else {
        prv_fb_enable_cursor();
    }
}

bool framebuf_is_history_mode_active(void) {
    if (!g_fb_shadow_buf) { return false; }

    return g_fb_start_at_sh_row <
           (FRAMEBUF_NUM_SHADOW_SCREENS - 1) * g_fb_height_ch;
}

/**
 * Gets the framebuffer position for a character on the last shadow screen.
 *
 * @param      sh_row    Row of the character on the last shadow buffer.
 * @param      sh_col    Column of the character on the last shadow buffer.
 * @param[out] p_fb_row  Where to save the row.
 * @param[out] p_fb_col  Where to save the column.
 *
 * @returns `true` if the position is currently visible and @a *p_fb_row and
 * @a *p_fb_col have been set.
 */
static bool prv_fb_get_buf_idx(size_t sh_row, size_t sh_col, size_t *p_fb_row,
                               size_t *p_fb_col) {
    if (sh_row >= g_fb_start_at_sh_row &&
        sh_row < g_fb_start_at_sh_row + g_fb_height_ch) {
        *p_fb_row = sh_row - g_fb_start_at_sh_row;
        *p_fb_col = sh_col;
        return true;
    } else {
        return false;
    }
}

/**
 * Gets the framebuffer row range for a range of rows on the last shadow buffer.
 *
 * Similar to #prv_fb_get_buf_idx(), except this one converts the range of rows
 * on the last shadow screen to their corresponding visible rows.
 *
 * @param      lss_start_row   First row of the shadow buffer range.
 * @param      lss_num_rows    Number of rows in the shadow buffer range.
 * @param[out] p_fb_start_row  First corresponding row on the framebuffer.
 * @param[out] p_fb_num_rows   Number of rows in the corresponding framebuffer
 *                             range (may be less than @a lss_num_rows due to
 *                             clipping).
 */
static void prv_fb_get_buf_range(size_t lss_start_row, size_t lss_num_rows,
                                 size_t *p_fb_start_row,
                                 size_t *p_fb_num_rows) {
    size_t sh_start_row =
        (FRAMEBUF_NUM_SHADOW_SCREENS - 1) * g_fb_height_ch + lss_start_row;
    size_t sh_end_row = sh_start_row + lss_num_rows;

    // TODO: check if this is the right logic?
    if (sh_start_row >= g_fb_start_at_sh_row) {
        *p_fb_start_row = sh_start_row - g_fb_start_at_sh_row;

        if (sh_end_row > g_fb_start_at_sh_row + g_fb_height_ch) {
            *p_fb_num_rows =
                lss_num_rows -
                (sh_end_row - (g_fb_start_at_sh_row + g_fb_height_ch));
        } else {
            *p_fb_num_rows = lss_num_rows;
        }
    } else {
        *p_fb_start_row = 0;
        *p_fb_num_rows = 0;
    }
}

/**
 * Draws a glyph only in the shadow buffer.
 *
 * @note
 * This function does not update the framebuffer even if the position is
 * visible.
 */
static void prv_fb_draw_sh_glyph(size_t sh_row, size_t sh_col, char ch) {
    size_t sh_y = sh_row * g_fb_font.height_px;
    size_t sh_x = sh_col * g_fb_font.width_px;
    prv_fb_draw_glyph_in(g_fb_shadow_buf, sh_y, sh_x, ch);
}

/**
 * Draws a (visible) glyph only on the framebuffer.
 *
 * @note
 * This function does not update the shadow buffer.
 */
static void prv_fb_draw_real_glyph(size_t fb_row, size_t fb_col, char ch) {
    size_t fb_y = fb_row * g_fb_font.height_px;
    size_t fb_x = fb_col * g_fb_font.width_px;
    prv_fb_draw_glyph_in(gp_framebuf, fb_y, fb_x, ch);
}

/**
 * Draws a glyph in the specified buffer.
 *
 * @param p_buf  Buffer with enough size to place a glyph at (y; x).
 * @param y      Row.
 * @param x      Column.
 * @param ch     Character to draw.
 */
static void prv_fb_draw_glyph_in(uint8_t *p_buf, size_t y, size_t x, char ch) {
    const uint8_t *p_glyph = psf_glyph(&g_fb_font, ch);
    for (size_t it_y = 0; it_y < g_fb_font.height_px; it_y++) {
        for (size_t it_x = 0; it_x < g_fb_font.width_px; it_x++) {
            uint8_t val = p_glyph[it_y] & (1 << (7 - it_x));
            if (val) {
                prv_fb_draw_pixel(p_buf, y + it_y, x + it_x);
            } else {
                prv_fb_clear_pixel(p_buf, y + it_y, x + it_x);
            }
        }
    }
}

/**
 * Draws a white pixel at (y; x) in the buffer @a p_buf.
 */
static void prv_fb_draw_pixel(uint8_t *p_buf, size_t y, size_t x) {
    size_t offset = y * g_fb_pitch_px + x * (g_fb_bpp / 8);
    p_buf[offset + 0] = 0xFF;
    p_buf[offset + 1] = 0xFF;
    p_buf[offset + 2] = 0xFF;
}

/**
 * Draws a black pixel at (y; x) in the buffer @a p_buf.
 */
static void prv_fb_clear_pixel(uint8_t *p_buf, size_t y, size_t x) {
    size_t offset = y * g_fb_pitch_px + x * (g_fb_bpp / 8);
    kmemset(&p_buf[offset], 0, g_fb_bpp / 8);
}

/**
 * Inverts the pixel at (y; x) in the buffer @a p_buf.
 *
 * It uses logical NOT to get the inverted values for colors.
 */
static void prv_fb_invert_pixel(uint8_t *p_buf, size_t y, size_t x) {
    size_t offset = y * g_fb_pitch_px + x * (g_fb_bpp / 8);
    p_buf[offset + 0] = ~p_buf[offset + 0];
    p_buf[offset + 1] = ~p_buf[offset + 1];
    p_buf[offset + 2] = ~p_buf[offset + 2];
}

/**
 * Enable the cursor and immediately draw it on the framebuffer.
 */
static void prv_fb_enable_cursor(void) {
    g_fb_cursor_en = true;
    prv_fb_draw_real_cursor();
}

/**
 * Disable the cursor and redraw the character it has been placed upon.
 */
static void prv_fb_disable_cursor(void) {
    g_fb_cursor_en = false;
    prv_fb_copy_sh_to_real(g_fb_cursor_lss_row, 1);
}

/**
 * Draw the cursor at its current position.
 *
 * The cursor position is controlled by #g_fb_cursor_lss_row and
 * #g_fb_cursor_lss_col.
 */
static void prv_fb_draw_real_cursor(void) {
    size_t sh_row = (FRAMEBUF_NUM_SHADOW_SCREENS - 1) * g_fb_height_ch +
                    g_fb_cursor_lss_row;
    size_t sh_col = g_fb_cursor_lss_col;

    size_t fb_row;
    size_t fb_col;
    bool b_visible = prv_fb_get_buf_idx(sh_row, sh_col, &fb_row, &fb_col);
    if (b_visible) {
        size_t fb_y = fb_row * g_fb_font.height_px;
        size_t fb_x = fb_col * g_fb_font.width_px;
        for (size_t it_y = 0; it_y < g_fb_font.height_px; it_y++) {
            for (size_t it_x = 0; it_x < g_fb_font.width_px; it_x++) {
                prv_fb_invert_pixel(gp_framebuf, fb_y + it_y, fb_x + it_x);
            }
        }
    }
}

/**
 * Copy rows from the shadow buffer to the framebuffer.
 *
 * Rows in the shadow buffer are counted starting from #g_fb_start_at_sh_row.
 */
static void prv_fb_copy_sh_to_real(size_t fb_start_row, size_t fb_num_rows) {
    kmemmove_sse2(&gp_framebuf[fb_start_row * g_fb_pitch_row],
                  &g_fb_shadow_buf[(g_fb_start_at_sh_row + fb_start_row) *
                                   g_fb_pitch_row],
                  fb_num_rows * g_fb_pitch_row);
}
