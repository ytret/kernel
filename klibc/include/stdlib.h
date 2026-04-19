#pragma once

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

[[noreturn]] void abort(void);
[[noreturn]] void exit(int status);

char *getenv(const char *name);

void *realloc(void *p, size_t size);
void free(void *p);

double strtod(const char *restrict nptr, char **endptr);
