#include "kprintf.h"
#include "psf.h"

#define HEADER_MAGIC 0x864AB572

typedef struct [[gnu::packed]] {
    uint32_t magic;
    uint32_t version;
    uint32_t hdr_size;
    uint32_t flags;
    uint32_t num_glyphs;
    uint32_t glyph_size;
    uint32_t height_px;
    uint32_t width_px;
} hdr_t;

bool psf_load(psf_t *p_font, uint32_t addr) {
    hdr_t const *p_hdr = (hdr_t const *)addr;

    if ((p_hdr->magic != HEADER_MAGIC) || (p_hdr->version != 0) ||
        (p_hdr->hdr_size != sizeof(hdr_t)) || (p_hdr->height_px != 16) ||
        (p_hdr->width_px != 8) ||
        (p_hdr->glyph_size != ((p_hdr->height_px * p_hdr->width_px) / 8))) {
        return false;
    }

    p_font->p_glyphs = (((uint8_t *)addr) + sizeof(hdr_t));
    p_font->num_glyphs = p_hdr->num_glyphs;
    p_font->glyph_size = p_hdr->glyph_size;
    p_font->height_px = p_hdr->height_px;
    p_font->width_px = p_hdr->width_px;

    return true;
}

uint8_t const *psf_glyph(psf_t const *p_psf, char ch) {
    if (((size_t)ch) >= p_psf->num_glyphs) {
        kprintf("psf_glyph: glyph for char %u is absent\n", ((uint32_t)ch));
    }

    size_t offset = (ch * p_psf->glyph_size);
    return &p_psf->p_glyphs[offset];
}
