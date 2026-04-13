#include <ytalloc/ytalloc.h>

#include "kstring.h"
#include "libshim.h"
#include "log.h"
#include "memfun.h"
#include "panic.h"

#define LIBSHIM_MAX_FMT_SIZE 128

static char g_libshim_fmt[LIBSHIM_MAX_FMT_SIZE];

static void prv_libshim_abort(void);
static int prv_libshim_log(const char *fmt, va_list ap);

void libshim_init(void) {
    alloc_set_abort_fn(prv_libshim_abort);
    alloc_set_log_fn(prv_libshim_log);
}

static void prv_libshim_abort(void) {
    PANIC("abort callback called from a libary");
}

static int prv_libshim_log(const char *fmt, va_list ap) {
    const size_t fmt_len = string_len(fmt);
    kmemcpy(g_libshim_fmt, fmt, fmt_len + 1);
    if (fmt_len > 0 && fmt[fmt_len - 1] == '\n') {
        g_libshim_fmt[fmt_len - 1] = '\0';
    }
    return log_vprintf(__FILE_NAME__, __func__, __LINE__, LOG_LEVEL_ERROR,
                       g_libshim_fmt, ap);
}
