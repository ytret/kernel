#pragma once

#include <stdarg.h>

int kprintf(char const *restrict p_format, ...);
int kvprintf(char const *restrict p_format, va_list args);
