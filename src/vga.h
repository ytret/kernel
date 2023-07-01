#pragma once

#include <stddef.h>

void vga_clear(void);
void vga_print_str(char const * p_str);
void vga_print_str_len(char const * p_str, size_t len);
