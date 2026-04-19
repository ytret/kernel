#pragma once

#include <stdarg.h>
#include <stddef.h>

int kprintf(char const *restrict fmt, ...);
int kvprintf(char const *restrict fmt, va_list ap);
int ksnprintf(char *str, size_t size, const char *fmt, ...);
int kvsnprintf(char *str, size_t size, const char *fmt, va_list ap);
