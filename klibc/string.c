#include <string.h>
#include <ytkernel/kstring.h>
#include <ytkernel/memfun.h>
#include <ytkernel/panic.h>

void *memchr(const void *s, int c, size_t n) {
    PANIC("stub %s called", __func__);
}

char *strchr(const char *s, int c) {
    while (*s && *s != c)
        s++;
    if (*s == c) return (char *)s;
    return NULL;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
    }

    return (*(unsigned char *)s1) - (*(unsigned char *)s2);
}

int strncmp(const char s1[], const char s2[], size_t n) {
    PANIC("stub %s called", __func__);
}

char *strstr(const char *haystack, const char *needle) {
    PANIC("stub %s called", __func__);
}

size_t strspn(const char *s, const char *accept) {
    PANIC("stub %s called", __func__);
}

char *strcpy(char *restrict dst, const char *restrict src) {
    PANIC("stub %s called", __func__);
}

size_t strlen(const char *s) {
    return string_len(s);
}

char *strpbrk(const char *s, const char *accept) {
    PANIC("stub %s called", __func__);
}

int strcoll(const char *s1, const char *s2) {
    PANIC("stub %s called", __func__);
}

char *strerror(int errnum) {
    PANIC("stub %s called", __func__);
}
