#include <framebuf.h>
#include <mbi.h>
#include <panic.h>
#include <psf.h>

static uint8_t * gp_framebuf;
static uint32_t  g_pitch;
static uint8_t   g_bpp;

static size_t g_height_px;
static size_t g_width_px;
static size_t g_height_chars;
static size_t g_width_chars;

static psf_t g_font;

static void draw_glyph_at(size_t y, size_t x, char ch);
static void draw_pixel_at(size_t y, size_t x);
static void clear_pixel_at(size_t y, size_t x);

static void scroll_pixels(size_t num_px);

void
framebuf_init (mbi_t const * p_mbi)
{
    gp_framebuf = ((uint8_t *) ((uint32_t) p_mbi->framebuffer_addr));
    g_pitch     = p_mbi->framebuffer_pitch;
    g_bpp       = p_mbi->framebuffer_bpp;

    g_height_px = p_mbi->framebuffer_height;
    g_width_px  = p_mbi->framebuffer_width;

    // Load the font.
    //
    void const * p_psf_mod = mbi_find_mod(p_mbi, "font");
    if (!p_psf_mod)
    {
        panic_silent();
    }

    bool b_font_ok = psf_load(&g_font, p_psf_mod);
    if (!b_font_ok)
    {
        panic_silent();
    }

    g_height_chars = (g_height_px / g_font.height_px);
    g_width_chars = (g_width_px / g_font.width_px);
}

size_t
framebuf_height_chars (void)
{
    return (g_height_chars);
}

size_t
framebuf_width_chars (void)
{
    return (g_width_chars);
}

void
framebuf_put_char_at (size_t row, size_t col, char ch)
{
    if ((row >= g_height_chars) || (col >= g_width_chars))
    {
        panic_silent();
    }

    size_t y = (row * g_font.height_px);
    size_t x = (col * g_font.width_px);

    draw_glyph_at(y, x, ch);
}

void
framebuf_put_cursor_at (size_t row, size_t col)
{
    (void) row;
    (void) col;
}

void
framebuf_scroll (void)
{
    scroll_pixels(g_font.height_px);
}

static void
draw_glyph_at (size_t y, size_t x, char ch)
{
    uint8_t const * p_glyph = psf_glyph(&g_font, ch);
    for (size_t j = 0; j < g_font.height_px; j++)
    {
        for (size_t i = 0; i < g_font.width_px; i++)
        {
            uint8_t val = (p_glyph[j] & (1 << (7 - i)));
            if (val)
            {
                draw_pixel_at((y + j), (x + i));
            }
            else
            {
                clear_pixel_at((y + j), (x + i));
            }
        }
    }
}

static void
draw_pixel_at (size_t y, size_t x)
{
    size_t offset = ((y * g_pitch) + (x * (g_bpp / 8)));
    gp_framebuf[offset + 0] = 0xFF;
    gp_framebuf[offset + 1] = 0xFF;
    gp_framebuf[offset + 2] = 0xFF;
}

static void
clear_pixel_at (size_t y, size_t x)
{
    size_t offset = ((y * g_pitch) + (x * (g_bpp / 8)));
    __builtin_memset(&gp_framebuf[offset], 0, (g_bpp / 8));
}

static void
scroll_pixels (size_t num_px)
{
    // Move rows up.
    //
    __builtin_memmove(gp_framebuf,
                      &gp_framebuf[num_px * g_pitch],
                      (g_height_px - num_px) * g_pitch);

    // Clear the lower part.
    //
    __builtin_memset(&gp_framebuf[(g_height_px - num_px) * g_pitch],
                     0,
                     (num_px * g_pitch));
}
