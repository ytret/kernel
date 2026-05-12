/**
 * @file framebuf.h
 * Framebuffer terminal.
 *
 * See #output_impl_t in @link term.c @endlink for the terminal interface that
 * this module implements.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void framebuf_early_init(void);
void framebuf_init(void);
void framebuf_map_iomem(void);

uint32_t framebuf_start(void);
uint32_t framebuf_end(void);

size_t framebuf_height_chars(void);
size_t framebuf_width_chars(void);

void framebuf_put_char_at(size_t lss_row, size_t lss_col, char ch);
void framebuf_put_cursor_at(size_t lss_row, size_t lss_col);
void framebuf_clear_rows(size_t lss_start_row, size_t lss_num_rows);
void framebuf_scroll_new_row(void);
