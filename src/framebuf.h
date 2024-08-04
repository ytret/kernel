#pragma once

#include <stddef.h>
#include <stdint.h>

void framebuf_init(void);

uint32_t framebuf_start(void);
uint32_t framebuf_end(void);

size_t framebuf_height_chars(void);
size_t framebuf_width_chars(void);

void framebuf_put_char_at(size_t row, size_t col, char ch);
void framebuf_put_cursor_at(size_t row, size_t col);

void framebuf_clear_rows(size_t start_row, size_t num_rows);
void framebuf_scroll(void);
