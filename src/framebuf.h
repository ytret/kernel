#pragma once

#include <stddef.h>

void framebuf_init(void);

size_t framebuf_height_chars(void);
size_t framebuf_width_chars(void);

void framebuf_put_char_at(size_t row, size_t col, char ch);
void framebuf_put_cursor_at(size_t row, size_t col);
void framebuf_scroll(void);
