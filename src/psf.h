#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint8_t const * p_glyphs;

    size_t    num_glyphs;
    size_t    glyph_size;
    size_t    height_px;
    size_t    width_px;
} psf_t;

bool            psf_load(psf_t * p_font, uint32_t addr);
uint8_t const * psf_glyph(psf_t const * p_font, char ch);
