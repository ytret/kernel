#include "kprintf.h"
#include "log.h"
#include "serial.h"

#define LOG_BUF_SIZE 256

// FIXME: both kprintf and this module have separate buffers, but they have the
// same contents.
static char g_log_buf[LOG_BUF_SIZE];

static const char *prv_log_level_str(int level);

void log_printf(const char *file, const char *func, int line, int level,
                const char *fmt, ...) {
    const char *level_str;
    va_list args;
    va_start(args, fmt);

    level_str = prv_log_level_str(level);

    // Note the differences: kprintf output has `file` instead of `func` for
    // brevity, and has less formatting.
    kprintf("%s %s:%d ", level_str, file, line);
    ksnprintf(g_log_buf, LOG_BUF_SIZE, "%s %27s:%04d ", level_str, func, line);
    serial_puts(g_log_buf);

    kvprintf(fmt, args);
    kvsnprintf(g_log_buf, LOG_BUF_SIZE, fmt, args);
    serial_puts(g_log_buf);

    kprintf("\n");
    serial_puts("\n");

    va_end(args);
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
