#include <locale.h>
#include <ytkernel/panic.h>

struct lconv *localeconv(void) {
    PANIC("stub %s called", __func__);
}

char *setlocale(int category, const char *locale) {
    (void)category;
    (void)locale;
    PANIC("stub %s called", __func__);
}
