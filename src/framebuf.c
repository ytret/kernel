#include "assert.h"
#include "framebuf.h"
#include "heap.h"
#include "kinttypes.h"
#include "log.h"
#include "mbi.h"
#include "memfun.h"
#include "psf.h"
#include "vmm.h"

static const textdisp_ops_t g_fb_disp_ops = {
    .p_init = framebuf_init,
    .p_map_iomem = framebuf_map_iomem,
    .p_put_char_at = framebuf_put_char_at,
    .p_put_cursor_at = framebuf_put_cursor_at,
    .p_clear_rows = framebuf_clear_rows,
    .p_scroll_new_row = framebuf_scroll_new_row,
};

static uint8_t *g_framebuf;

static uint32_t g_fb_height_px;
static uint32_t g_fb_width_px;
static uint32_t g_fb_pitch_px;
static uint8_t g_fb_bpp;

static uint32_t g_fb_height_ch;
static uint32_t g_fb_width_ch;
static uint32_t g_fb_row_pitch;

static char *g_fb_cache;
static size_t g_fb_cache_size;
static uint32_t g_fb_cache_row_pitch;

static size_t g_fb_cursor_row;
static size_t g_fb_cursor_col;

static psf_t g_fb_psf;

// See arch/x86/boot.s.
extern uint32_t framebuf_pgtbl[];

static void prv_fb_draw_glyph(uint8_t *buf, size_t row, size_t col, char ch);
static void prv_fb_invert_glyph(uint8_t *buf, size_t row, size_t col);
static void prv_fb_restore_glyph(uint8_t *buf, size_t row, size_t col);

static void prv_fb_draw_pixel(uint8_t *buf, size_t y, size_t x);
static void prv_fb_clear_pixel(uint8_t *buf, size_t y, size_t x);
static void prv_fb_invert_pixel(uint8_t *buf, size_t y, size_t x);

const textdisp_ops_t *framebuf_textdisp_ops(void) {
    return &g_fb_disp_ops;
};

void framebuf_early_init(void) {
    const mbi_t *p_mbi = mbi_ptr();

    g_framebuf = (uint8_t *)((uint32_t)p_mbi->framebuffer_addr);
    g_fb_height_px = p_mbi->framebuffer_height;
    g_fb_width_px = p_mbi->framebuffer_width;
    g_fb_pitch_px = p_mbi->framebuffer_pitch;
    g_fb_bpp = p_mbi->framebuffer_bpp;

    const size_t fb_size = g_fb_pitch_px * g_fb_height_px;

    LOG_DEBUG("framebuf address %p", g_framebuf);
    LOG_DEBUG("size 0x%08" PRIx32, fb_size);
    LOG_DEBUG("height %zu width %zu", g_fb_height_px, g_fb_width_px);
    LOG_DEBUG("pitch %" PRIu32 " bpp %u", g_fb_pitch_px, g_fb_bpp);

    const bool fb_mapped =
        vmm_kmap_region_in(framebuf_pgtbl, (uint32_t)g_framebuf, fb_size);
    ASSERT(fb_mapped);

    const mbi_mod_t *p_mod = mbi_find_mod("font");
    if (!p_mod) {
        PANIC("could not find the 'font' module for the framebuffer terminal");
    }

    const bool loaded_psf = psf_load(&g_fb_psf, p_mod->mod_start);
    ASSERT(loaded_psf);

    g_fb_height_ch = g_fb_height_px / g_fb_psf.height_px;
    g_fb_width_ch = g_fb_width_px / g_fb_psf.width_px;
    g_fb_row_pitch = g_fb_pitch_px * g_fb_psf.height_px;
}

void framebuf_init(void) {
    g_fb_cache_size = g_fb_width_ch * g_fb_height_ch;
    g_fb_cache = heap_alloc_aligned(g_fb_cache_size, PMM_PAGE_SIZE);
    kmemset(g_fb_cache, 0, g_fb_cache_size);

    g_fb_cache_row_pitch = g_fb_width_ch;
}

void framebuf_map_iomem(void) {
    const uint32_t iomem_start = (uint32_t)g_framebuf;
    const uint32_t iomem_end =
        (uint32_t)g_framebuf + g_fb_height_px * g_fb_pitch_px;

    LOG_DEBUG("map framebuf memory 0x%08" PRIx32 " .. 0x%08" PRIx32,
              iomem_start, iomem_end);
    for (uint32_t page = iomem_start; page < iomem_end; page += 4096) {
        vmm_map_kernel_page(page, page);
    }
}

size_t framebuf_height_chars(void) {
    return g_fb_height_ch;
}

size_t framebuf_width_chars(void) {
    return g_fb_width_ch;
}

void framebuf_put_char_at(size_t row, size_t col, char ch) {
    prv_fb_draw_glyph(g_framebuf, row, col, ch);
    if (g_fb_cache) { g_fb_cache[col + row * g_fb_cache_row_pitch] = ch; }
}

void framebuf_put_cursor_at(size_t row, size_t col) {
    if (!g_fb_cache) { return; }

    prv_fb_restore_glyph(g_framebuf, g_fb_cursor_row, g_fb_cursor_col);
    g_fb_cursor_row = row;
    g_fb_cursor_col = col;
    prv_fb_invert_glyph(g_framebuf, g_fb_cursor_row, g_fb_cursor_col);
}

void framebuf_clear_rows(size_t start_row, size_t num_rows) {
    kmemclr_sse2(&g_framebuf[start_row * g_fb_row_pitch],
                 num_rows * g_fb_row_pitch);
    kmemclr_sse2(&g_fb_cache[start_row * g_fb_cache_row_pitch],
                 num_rows * g_fb_cache_row_pitch);
}

void framebuf_scroll_new_row(void) {
    if (g_fb_cache) {
        // Remove the cursor so it doesn't get picked up by the memmoves.
        prv_fb_restore_glyph(g_framebuf, g_fb_cursor_row, g_fb_cursor_col);
    }

    // Move up every row except the first one.
    kmemmove_sse2(g_framebuf, &g_framebuf[g_fb_row_pitch],
                  g_fb_row_pitch * (g_fb_height_ch - 1));
    kmemmove_sse2(g_fb_cache, &g_fb_cache[g_fb_cache_row_pitch],
                  g_fb_cache_row_pitch * (g_fb_height_ch - 1));

    // Clear the last row.
    const size_t last_row = g_fb_height_ch - 1;
    kmemclr_sse2(&g_framebuf[last_row * g_fb_row_pitch], g_fb_row_pitch);
    kmemclr_sse2(&g_fb_cache[last_row * g_fb_cache_row_pitch],
                 g_fb_cache_row_pitch);

    if (g_fb_cache) {
        prv_fb_restore_glyph(g_framebuf, g_fb_cursor_row, g_fb_cursor_col);
    }
}

static void prv_fb_draw_glyph(uint8_t *buf, size_t row, size_t col, char ch) {
    DEBUG_ASSERT(row < g_fb_height_ch && col < g_fb_width_ch);

    const size_t y = row * g_fb_psf.height_px;
    const size_t x = col * g_fb_psf.width_px;
    const uint8_t *p_glyph = psf_glyph(&g_fb_psf, ch);

    for (size_t it_y = 0; it_y < g_fb_psf.height_px; it_y++) {
        for (size_t it_x = 0; it_x < g_fb_psf.width_px; it_x++) {
            uint8_t val = p_glyph[it_y] & (1 << (7 - it_x));
            if (val) {
                prv_fb_draw_pixel(buf, y + it_y, x + it_x);
            } else {
                prv_fb_clear_pixel(buf, y + it_y, x + it_x);
            }
        }
    }
}

static void prv_fb_invert_glyph(uint8_t *buf, size_t row, size_t col) {
    const size_t fb_y = row * g_fb_psf.height_px;
    const size_t fb_x = col * g_fb_psf.width_px;
    for (size_t it_y = 0; it_y < g_fb_psf.height_px; it_y++) {
        for (size_t it_x = 0; it_x < g_fb_psf.width_px; it_x++) {
            prv_fb_invert_pixel(buf, fb_y + it_y, fb_x + it_x);
        }
    }
}

static void prv_fb_restore_glyph(uint8_t *buf, size_t row, size_t col) {
    char cached_ch = g_fb_cache[col + row * g_fb_cache_row_pitch];
    if (cached_ch == '\0') { cached_ch = ' '; }
    prv_fb_draw_glyph(buf, row, col, cached_ch);
}

static void prv_fb_draw_pixel(uint8_t *buf, size_t y, size_t x) {
    const size_t offset = y * g_fb_pitch_px + x * (g_fb_bpp / 8);
    buf[offset + 0] = 0xFF;
    buf[offset + 1] = 0xFF;
    buf[offset + 2] = 0xFF;
}

static void prv_fb_clear_pixel(uint8_t *buf, size_t y, size_t x) {
    const size_t offset = y * g_fb_pitch_px + x * (g_fb_bpp / 8);
    kmemset(&buf[offset], 0, g_fb_bpp / 8);
}

static void prv_fb_invert_pixel(uint8_t *p_buf, size_t y, size_t x) {
    size_t offset = y * g_fb_pitch_px + x * (g_fb_bpp / 8);
    p_buf[offset + 0] = ~p_buf[offset + 0];
    p_buf[offset + 1] = ~p_buf[offset + 1];
    p_buf[offset + 2] = ~p_buf[offset + 2];
}
