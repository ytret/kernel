#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <ytprintf.h>

#include "console.h"
#include "kprintf.h"

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
    int ret;
    va_list ap_copy;
    console_t *const con = console_get_boot_con();
    if (!console_is_ready(con)) { return -1; }

    // NOTE: it is important to copy `ap`, otherwise the internal argument
    // pointer/counter is never advanced.
    va_copy(ap_copy, ap);

    console_lock(con);
    ret = yt_vsnprintf(g_kprintf_buf, KPRINTF_BUF_SIZE, fmt, ap_copy);
    if (ret > KPRINTF_BUF_SIZE) { ret = KPRINTF_BUF_SIZE; }
    console_put_str(con, g_kprintf_buf);
    console_unlock(con);

    va_end(ap_copy);
    return ret;
}

int ksnprintf(char *str, size_t size, const char *fmt, ...) {
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = kvsnprintf(str, size, fmt, ap);
    va_end(ap);

    return ret;
}

int kvsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
    int ret;
    va_list ap_copy;

    va_copy(ap_copy, ap);
    ret = yt_vsnprintf(str, size, fmt, ap_copy);

    va_end(ap_copy);
    return ret;
}

int klsnprintf(char *str, size_t size, const char *fmt, ...) {
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = kvlsnprintf(str, size, fmt, ap);
    va_end(ap);

    return ret;
}

int kvlsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
    int ret = kvsnprintf(str, size, fmt, ap);
    if (ret >= (int)size) { return (int)size; }
    return ret;
}
