/**
 * @file framebuf.c
 * Framebuffer terminal implementation.
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

static uint8_t *gp_framebuf;
static uint8_t *g_fb_shbuf;
static psf_t g_fb_font;

static size_t g_fb_height_px;
static size_t g_fb_height_ch;
static size_t g_fb_width_px;
static size_t g_fb_width_ch;

static uint32_t g_fb_pitch_px;
static uint32_t g_fb_pitch_row;
static uint8_t g_fb_bpp;

static bool g_fb_cursor_en = true;
static size_t g_fb_cursor_row;
static size_t g_fb_cursor_col;

// See arch/x86/boot.s.
extern uint32_t framebuf_pgtbl[];

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

void framebuf_early_init(void) {
    const mbi_t *p_mbi = mbi_ptr();

    gp_framebuf = (uint8_t *)((uint32_t)p_mbi->framebuffer_addr);
    g_fb_height_px = p_mbi->framebuffer_height;
    g_fb_width_px = p_mbi->framebuffer_width;
    g_fb_pitch_px = p_mbi->framebuffer_pitch;
    g_fb_bpp = p_mbi->framebuffer_bpp;

    const size_t fb_size = g_fb_pitch_px * g_fb_height_px;

    LOG_DEBUG("framebuf address %p", gp_framebuf);
    LOG_DEBUG("size 0x%08" PRIx32, fb_size);
    LOG_DEBUG("height %zu width %zu", g_fb_height_px, g_fb_width_px);
    LOG_DEBUG("pitch %" PRIu32 " bpp %u", g_fb_pitch_px, g_fb_bpp);

    const bool fb_mapped =
        vmm_kmap_region_in(framebuf_pgtbl, (uint32_t)gp_framebuf, fb_size);
    ASSERT(fb_mapped);

    const mbi_mod_t *p_mod = mbi_find_mod("font");
    if (!p_mod) {
        PANIC("could not find the 'font' module for the framebuffer terminal");
    }

    bool b_psf_loaded = psf_load(&g_fb_font, p_mod->mod_start);
    ASSERT(b_psf_loaded);

    g_fb_height_ch = g_fb_height_px / g_fb_font.height_px;
    g_fb_width_ch = g_fb_width_px / g_fb_font.width_px;
    g_fb_pitch_row = g_fb_pitch_px * g_fb_font.height_px;
}

void framebuf_init(void) {
    g_fb_shbuf = heap_alloc_aligned(g_fb_height_px * g_fb_pitch_px, 16);
    LOG_DEBUG("shadow buffer at %p", g_fb_shbuf);
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

void framebuf_put_char_at(size_t row, size_t col, char ch) {
    if (!g_fb_shbuf) { return; }

    ASSERT(row < g_fb_height_ch && col < g_fb_width_ch);

    prv_fb_draw_sh_glyph(row, col, ch);
    prv_fb_draw_real_glyph(row, col, ch);
}

void framebuf_put_cursor_at(size_t lss_row, size_t lss_col) {
    if (!g_fb_shbuf) { return; }

    // Remove the previous cursor.
    prv_fb_copy_sh_to_real(g_fb_cursor_row, 1);

    g_fb_cursor_row = lss_row;
    g_fb_cursor_col = lss_col;

    if (g_fb_cursor_en) { prv_fb_draw_real_cursor(); }
}

void framebuf_clear_rows(size_t start_row, size_t num_rows) {
    if (!g_fb_shbuf) { return; }

    const size_t num_bytes = num_rows * g_fb_pitch_row;
    kmemclr_sse2(&g_fb_shbuf[start_row * g_fb_pitch_row], num_bytes);

    kmemclr_sse2(&gp_framebuf[start_row * g_fb_pitch_row],
                 num_rows * g_fb_pitch_row);
}

void framebuf_scroll_new_row(void) {
    if (!g_fb_shbuf) { return; }

    // Move up every shadow buffer row except the first one.
    kmemmove_sse2(g_fb_shbuf, &g_fb_shbuf[1 * g_fb_pitch_row],
                  g_fb_pitch_row * (g_fb_height_ch - 1));

    // Clear the last shadow buffer row.
    const size_t last_row = g_fb_height_ch - 1;
    kmemclr_sse2(&g_fb_shbuf[last_row * g_fb_pitch_row], g_fb_pitch_row);

    prv_fb_copy_sh_to_real(0, g_fb_height_ch);
}

/**
 * Draws a glyph only in the shadow buffer.
 *
 * @note
 * This function does not update the framebuffer.
 */
static void prv_fb_draw_sh_glyph(size_t sh_row, size_t sh_col, char ch) {
    size_t sh_y = sh_row * g_fb_font.height_px;
    size_t sh_x = sh_col * g_fb_font.width_px;
    prv_fb_draw_glyph_in(g_fb_shbuf, sh_y, sh_x, ch);
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
[[gnu::unused]]
static void prv_fb_enable_cursor(void) {
    g_fb_cursor_en = true;
    prv_fb_draw_real_cursor();
}

/**
 * Disable the cursor and redraw the character it has been placed upon.
 */
[[gnu::unused]]
static void prv_fb_disable_cursor(void) {
    g_fb_cursor_en = false;
    prv_fb_copy_sh_to_real(g_fb_cursor_row, 1);
}

/**
 * Draw the cursor at its current position.
 *
 * The cursor position is controlled by #g_fb_cursor_row and #g_fb_cursor_col.
 */
static void prv_fb_draw_real_cursor(void) {
    const size_t row = g_fb_cursor_row;
    const size_t col = g_fb_cursor_col;

    const size_t fb_y = row * g_fb_font.height_px;
    const size_t fb_x = col * g_fb_font.width_px;
    for (size_t it_y = 0; it_y < g_fb_font.height_px; it_y++) {
        for (size_t it_x = 0; it_x < g_fb_font.width_px; it_x++) {
            prv_fb_invert_pixel(gp_framebuf, fb_y + it_y, fb_x + it_x);
        }
    }
}

/**
 * Copy rows from the shadow buffer to the framebuffer.
 */
static void prv_fb_copy_sh_to_real(size_t fb_start_row, size_t fb_num_rows) {
    kmemmove_sse2(&gp_framebuf[fb_start_row * g_fb_pitch_row],
                  &g_fb_shbuf[fb_start_row * g_fb_pitch_row],
                  fb_num_rows * g_fb_pitch_row);
}
