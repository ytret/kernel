#include <string.h>
#include <ytkernel/kstring.h>
#include <ytkernel/memfun.h>
#include <ytkernel/panic.h>

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *src = (const unsigned char *)s;
    unsigned char d = c;

    while (n--) {
        if (*src == d) return (void *)src;
        src++;
    }

    return NULL;
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
    const char *orig_s = s;
    const char *c;

    while (*s) {
        for (c = accept; *c; c++) {
            if (*s == *c) goto found;
        }
        if (*c == '\0') break;
    found:
        s++;
    }

    return s - orig_s;
}

char *strcpy(char *restrict dst, const char *restrict src) {
    char *s = dst;

    while ((*dst++ = *src++))
        ;

    return s;
}

size_t strlen(const char *s) {
    return string_len(s);
}

char *strpbrk(const char *s, const char *accept) {
    const char *c = accept;

    while (*s) {
        for (c = accept; *c; c++) {
            if (*s == *c) return (char *)s;
        }
        s++;
    }

    return (char *)NULL;
}

int strcoll(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}

char *strerror(int errnum) {
    PANIC("stub %s called", __func__);
}
