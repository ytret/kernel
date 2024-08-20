#pragma once

#include <stdbool.h>
#include <stddef.h>

void vga_init(void);

size_t vga_height_chars(void);
size_t vga_width_chars(void);

void vga_put_char_at(size_t row, size_t col, char ch);
void vga_put_cursor_at(size_t row, size_t col);
void vga_enable_cursor(void);
void vga_disable_cursor(void);

void vga_clear_rows(size_t start_row, size_t num_rows);
void vga_scroll_new_row(void);

void vga_clear_history(void);
size_t vga_history_screens(void);
size_t vga_history_pos(void);
void vga_set_history_pos(size_t row_from_start);
