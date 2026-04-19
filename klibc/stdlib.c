#include <stdlib.h>
#include <ytkernel/panic.h>

[[noreturn]] void abort(void) {
    PANIC("stub %s called", __func__);
}

[[noreturn]] void exit(int status) {
    PANIC("stub %s called", __func__);
}

char *getenv(const char *name) {
    PANIC("stub %s called", __func__);
}

void *realloc(void *p, size_t size) {
    PANIC("stub %s called", __func__);
}

void free(void *p) {
    PANIC("stub %s called", __func__);
}

double strtod(const char *restrict nptr, char **endptr) {
    PANIC("stub %s called", __func__);
}
