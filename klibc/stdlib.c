#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DEBUG
#endif

#include <stdlib.h>
#include <ytkernel/heap.h>
#include <ytkernel/panic.h>

#include <ytkernel/log.h>

[[noreturn]] void abort(void) {
    PANIC("klibc abort");
}

[[noreturn]] void exit(int status) {
    (void)status;
    PANIC("stub %s called", __func__);
}

char *getenv(const char *name) {
    (void)name;
    LOG_FLOW("name %s", name);
    char *const ret = NULL;
    LOG_FLOW("return str %p", ret);
    return ret;
}

void *realloc(void *p, size_t size) {
    LOG_FLOW("klibc realloc(%p, %zu)", p, size);
    return heap_realloc(p, size, 8);
}

void free(void *p) {
    heap_free(p);
}

double strtod(const char *restrict nptr, char **restrict endptr) {
    if (endptr) { *endptr = (char *)nptr; }
    LOG_ERROR("stub strtod not implemented, nptr = %s", nptr);
    return 0.0;
}
