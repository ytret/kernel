#pragma once

#include <stdint.h>

void vga_init(void);
void vga_put_char_at(uint8_t row, uint8_t col, char ch);
void vga_put_cursor_at(uint8_t row, uint8_t col);
void vga_scroll(void);
