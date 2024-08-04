#include "framebuf.h"
#include "mbi.h"
#include "memfun.h"
#include "panic.h"
#include "psf.h"

// Maximum supported PSF glyph width.
#define MAX_FONT_WIDTH_PX 40

static uint8_t *gp_framebuf;
static uint32_t g_pitch;
static uint8_t g_bpp;

static size_t g_height_px;
static size_t g_width_px;
static size_t g_height_chars;
static size_t g_width_chars;

static psf_t g_font;

static bool gb_cursor_off_screen = false;
static size_t g_cursor_row;
static size_t g_cursor_col;
static bool gpb_cursor_glyph_pxval[MAX_FONT_WIDTH_PX];

static void draw_glyph_at(size_t y, size_t x, char ch, bool b_overrides_cursor);
static void draw_pixel_at(size_t y, size_t x);
static void clear_pixel_at(size_t y, size_t x);
static bool get_pixel_at(size_t y, size_t x);

static void scroll_pixels(size_t num_px);

void framebuf_init(void) {
    mbi_t const *p_mbi = mbi_ptr();

    gp_framebuf = (uint8_t *)((uint32_t)p_mbi->framebuffer_addr);
    g_pitch = p_mbi->framebuffer_pitch;
    g_bpp = p_mbi->framebuffer_bpp;

    g_height_px = p_mbi->framebuffer_height;
    g_width_px = p_mbi->framebuffer_width;

    // Load the font.
    mbi_mod_t const *p_mod = mbi_find_mod("font");
    if (!p_mod) { panic_silent(); }

    bool b_psf_loaded = psf_load(&g_font, p_mod->mod_start);
    if (!b_psf_loaded) { panic_silent(); }

    if (g_font.width_px > MAX_FONT_WIDTH_PX) { panic_silent(); }

    g_height_chars = g_height_px / g_font.height_px;
    g_width_chars = g_width_px / g_font.width_px;
}

uint32_t framebuf_start(void) {
    return (uint32_t)gp_framebuf;
}

uint32_t framebuf_end(void) {
    uint32_t start = framebuf_start();
    return start + g_height_px * g_pitch;
}

size_t framebuf_height_chars(void) {
    return g_height_chars;
}

size_t framebuf_width_chars(void) {
    return g_width_chars;
}

void framebuf_put_char_at(size_t row, size_t col, char ch) {
    if ((row >= g_height_chars) || (col >= g_width_chars)) { panic_silent(); }

    size_t y = row * g_font.height_px;
    size_t x = col * g_font.width_px;

    // If the glyph overrides the cursor pixels, save the glyph pixels.
    bool b_overrides_cursor = row == g_cursor_row && col == g_cursor_col;

    draw_glyph_at(y, x, ch, b_overrides_cursor);
}

void framebuf_put_cursor_at(size_t row, size_t col) {
    if ((row >= g_height_chars) || (col >= g_width_chars)) { panic_silent(); }

    // Remove the previous cursor by restoring the glyph pixels.
    size_t prev_y = g_cursor_row * g_font.height_px + g_font.height_px - 1;
    size_t prev_x = g_cursor_col * g_font.width_px;
    if (!gb_cursor_off_screen) {
        for (size_t i = 0; i < g_font.width_px; i++) {
            if (gpb_cursor_glyph_pxval[i]) {
                draw_pixel_at(prev_y, prev_x + i);
            } else {
                clear_pixel_at(prev_y, prev_x + i);
            }
        }
    }

    // Put the cursor at the new position.
    size_t y = row * g_font.height_px + g_font.height_px - 1;
    size_t x = col * g_font.width_px;

    for (size_t i = 0; i < g_font.width_px; i++) {
        // In case there is already a glyph, save its pixels.
        gpb_cursor_glyph_pxval[i] = get_pixel_at(y, x + i);

        // Draw the cursor pixel.
        draw_pixel_at(y, x + i);
    }

    // Save the new position.
    gb_cursor_off_screen = false;
    g_cursor_row = row;
    g_cursor_col = col;
}

void framebuf_scroll(void) {
    if (g_cursor_row > 0) {
        g_cursor_row--;
    } else {
        gb_cursor_off_screen = true;
    }

    scroll_pixels(g_font.height_px);
}

static void draw_glyph_at(size_t y, size_t x, char ch,
                          bool b_overrides_cursor) {
    uint8_t const *p_glyph = psf_glyph(&g_font, ch);

    for (size_t j = 0; j < g_font.height_px; j++) {
        for (size_t i = 0; i < g_font.width_px; i++) {
            uint8_t val = p_glyph[j] & (1 << (7 - i));

            if (val) {
                draw_pixel_at(y + j, x + i);

                if (b_overrides_cursor && j == g_font.height_px - 1) {
                    gpb_cursor_glyph_pxval[i] = true;
                }
            } else {
                clear_pixel_at(y + j, x + i);

                if (b_overrides_cursor && j == g_font.height_px - 1) {
                    gpb_cursor_glyph_pxval[i] = false;
                }
            }
        }
    }
}

static void draw_pixel_at(size_t y, size_t x) {
    size_t offset = y * g_pitch + x * (g_bpp / 8);
    gp_framebuf[offset + 0] = 0xFF;
    gp_framebuf[offset + 1] = 0xFF;
    gp_framebuf[offset + 2] = 0xFF;
}

static void clear_pixel_at(size_t y, size_t x) {
    size_t offset = y * g_pitch + x * (g_bpp / 8);
    __builtin_memset(&gp_framebuf[offset], 0, g_bpp / 8);
}

static bool get_pixel_at(size_t y, size_t x) {
    size_t offset = y * g_pitch + x * (g_bpp / 8);
    return !(gp_framebuf[offset + 0] == 0 && gp_framebuf[offset + 1] == 0 &&
             gp_framebuf[offset + 2] == 0);
}

static void scroll_pixels(size_t num_px) {
    // Move rows up.
    memmove_sse2(gp_framebuf, &gp_framebuf[num_px * g_pitch],
                 (g_height_px - num_px) * g_pitch);

    // Clear the lower part.
    __builtin_memset(&gp_framebuf[(g_height_px - num_px) * g_pitch], 0,
                     num_px * g_pitch);
}
