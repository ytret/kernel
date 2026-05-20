#pragma once

#include <stdarg.h>
#include <stddef.h>

int kprintf(char const *restrict fmt, ...)
    __attribute__((format(printf, 1, 2)));
int kvprintf(char const *restrict fmt, va_list ap);
int ksnprintf(char *str, size_t size, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
int kvsnprintf(char *str, size_t size, const char *fmt, va_list ap);
int klsnprintf(char *str, size_t size, const char *fmt, ...);
int kvlsnprintf(char *str, size_t size, const char *fmt, va_list ap);
