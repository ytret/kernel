#pragma once

#include <stddef.h>

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

char *strchr(const char *s, int c);
int strcmp(const char *s1, const char *s2);
int strncmp(const char s1[], const char s2[], size_t n);
char *strstr(const char *haystack, const char *needle);
size_t strspn(const char *s, const char *accept);
char *strcpy(char *restrict dst, const char *restrict src);
size_t strlen(const char *s);
char *strpbrk(const char *s, const char *accept);
int strcoll(const char *s1, const char *s2);
char *strerror(int errnum);
