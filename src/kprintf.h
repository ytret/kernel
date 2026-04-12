#pragma once

#include <stdarg.h>
#include <stddef.h>

int kprintf(char const *restrict fmt, ...);
int kvprintf(char const *restrict fmt, va_list ap);
