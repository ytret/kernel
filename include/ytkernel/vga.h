/**
 * @file vga.h
 * VGA text-mode terminal.
 *
 * See #output_impl_t in @link term.c @endlink for the terminal interface that
 * this module implements.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

void vga_early_init(void);
void vga_map_iomem(void);

size_t vga_height_chars(void);
size_t vga_width_chars(void);

void vga_put_char_at(size_t row, size_t col, char ch);
void vga_put_cursor_at(size_t row, size_t col);
void vga_clear_rows(size_t start_row, size_t num_rows);

/**
 * Scrolls the screen so that one empty row is available at the bottom.
 */
void vga_scroll_new_row(void);
