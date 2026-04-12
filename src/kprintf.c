#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <ytprintf.h>

#include "kprintf.h"
#include "term.h"
#include "serial.h"

#define KPRINTF_BUF_SIZE 256

static char g_kprintf_buf[KPRINTF_BUF_SIZE];

int kprintf(char const *restrict fmt, ...) {
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = kvprintf(fmt, ap);
    va_end(ap);

    return ret;
}

int kvprintf(char const *restrict fmt, va_list ap) {
    bool b_release_mutex = false;
    int ret;
    va_list ap_copy;

    // NOTE: it is important to copy `ap`, otherwise the internal argument
    // pointer/counter is never advanced.
    va_copy(ap_copy, ap);

    if (!term_owns_mutex()) {
        term_acquire_mutex();
        b_release_mutex = true;
    }

    ret = yt_vsnprintf(g_kprintf_buf, KPRINTF_BUF_SIZE, fmt, ap_copy);
    if (ret > KPRINTF_BUF_SIZE) { ret = KPRINTF_BUF_SIZE; }

    term_print_str(g_kprintf_buf);

    if (b_release_mutex) { term_release_mutex(); }

    va_end(ap_copy);
    return ret;
}
