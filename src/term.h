#pragma once

#include <stddef.h>

void term_clear(void);

void term_print_str(char const * p_str);
void term_print_str_len(char const * p_str, size_t len);

void term_put_char_at(size_t row, size_t col, char ch);
void term_put_cursor_at(size_t row, size_t col);

size_t term_row(void);
size_t term_col(void);

size_t term_height(void);
size_t term_width(void);
