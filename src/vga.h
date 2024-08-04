#pragma once

#include <stddef.h>

void vga_init(void);

size_t vga_height_chars(void);
size_t vga_width_chars(void);

void vga_put_char_at(size_t row, size_t col, char ch);
void vga_put_cursor_at(size_t row, size_t col);

void vga_clear_rows(size_t start_row, size_t num_rows);
void vga_scroll(void);
