#include "config.h"

#include "kprintf.h"
#include "log.h"
#include "serial.h"

#define LOG_BUF_SIZE 256

// FIXME: both kprintf and this module have separate buffers, but they have the
// same contents.
static char g_log_buf[LOG_BUF_SIZE];

static const char *prv_log_level_str(int level);

int log_printf(const char *file, const char *func, int line, int level,
               const char *fmt, ...) {
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = log_vprintf(file, func, line, level, fmt, ap);
    va_end(ap);

    return ret;
}

int log_vprintf(const char *file, const char *func, int line, int level,
                const char *fmt, va_list ap) {
    int ret = 0;
    const char *level_str;

    level_str = prv_log_level_str(level);

    // Note the differences: kprintf output has `file` instead of `func` for
    // brevity, and has less formatting.
    if (level <= YTKERNEL_TERM_LOG_LEVEL) {
        kprintf("%s %s:%d ", level_str, file, line);
    }
    ret += ksnprintf(g_log_buf, LOG_BUF_SIZE, "%s %27s:%04d ", level_str, func,
                     line);
    serial_puts(g_log_buf);

    if (level <= YTKERNEL_TERM_LOG_LEVEL) { kvprintf(fmt, ap); }
    ret += kvsnprintf(g_log_buf, LOG_BUF_SIZE, fmt, ap);
    serial_puts(g_log_buf);

    if (level <= YTKERNEL_TERM_LOG_LEVEL) { kprintf("\n"); }
    serial_puts("\n");
    ret += 1;

    return ret;
}

static const char *prv_log_level_str(int level) {
    switch (level) {
    case LOG_LEVEL_ERROR:
        return "E";
    case LOG_LEVEL_INFO:
        return "I";
    case LOG_LEVEL_DEBUG:
        return "D";
    case LOG_LEVEL_FLOW:
        return "F";
    default:
        return "?";
    }
}
