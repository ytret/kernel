#include <lua.h>

#include "config.h"

#include "assert.h"
#include "kprintf.h"
#include "kspinlock.h"
#include "log.h"
#include "memfun.h"
#include "serial.h"

#define LOG_BUF_SIZE 256

// FIXME: both kprintf and this module have separate buffers, but they have the
// same contents.
static char g_log_buf[LOG_BUF_SIZE];

static spinlock_t g_log_lock;

static const char *prv_log_level_str(int level);

static int prv_log_lua_debug(lua_State *L);
static int prv_log_lua_debug_raw(lua_State *L);
static void prv_log_get_lua_caller(lua_State *L, char *file, const char **func,
                                   int *line);

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

    spinlock_acquire(&g_log_lock);

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

    spinlock_release(&g_log_lock);

    return ret;
}

int log_init_lua(void *v_L) {
    lua_State *L = v_L;
    int base = lua_gettop(L);
    int idx_kobj = base;
    ASSERT(idx_kobj > 0);

    lua_createtable(L, 0, 1);

    lua_pushcfunction(L, prv_log_lua_debug);
    lua_setfield(L, -2, "debug");

    lua_pushcfunction(L, prv_log_lua_debug_raw);
    lua_setfield(L, -2, "debug_raw");

    lua_setfield(L, idx_kobj, "log");

    ASSERT(lua_gettop(L) == base);
    return 0;
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

static int prv_log_lua_debug(lua_State *L) {
    int nargs = lua_gettop(L);

    // Get string.format().
    int val_type = lua_getglobal(L, "string");
    if (val_type == LUA_TNIL) {
        LOG_ERROR(
            "lua_getglobal returned nil for string, is the library opened?");
        return -1;
    }
    val_type = lua_getfield(L, -1, "format");
    if (val_type != LUA_TFUNCTION) {
        LOG_ERROR("lua_getfield pushed %s for \"string.format\"",
                  lua_typename(L, val_type));
        return -1;
    }
    lua_remove(L, -2);

    // Move string.format to index 1 so that it can be called with the same
    // arguments.
    lua_insert(L, 1);

    // Call string.format(...).
    int err = lua_pcall(L, nargs, 1, 0);
    if (err != LUA_OK) {
        const char *errstr = lua_tostring(L, -1);
        LOG_ERROR("pcall(string.format) returned %d", err);
        LOG_ERROR("error string: %s", errstr);
        return -1;
    }
    ASSERT(lua_gettop(L) > 0); // must return something

    char file[LUA_IDSIZE];
    const char *func;
    int line;
    prv_log_get_lua_caller(L, file, &func, &line);

    const char *s = lua_tostring(L, 1);
    log_printf(file, func, line, LOG_LEVEL_DEBUG, "%s", s);

    return 0;
}

static int prv_log_lua_debug_raw(lua_State *L) {
    // TODO: check nargs?

    char file[LUA_IDSIZE];
    const char *func;
    int line;
    prv_log_get_lua_caller(L, file, &func, &line);

    const char *s = lua_tostring(L, 1);
    log_printf(file, func, line, LOG_LEVEL_DEBUG, "%s", s);

    return 0;
}

static void prv_log_get_lua_caller(lua_State *L, char *file, const char **func,
                                   int *line) {
    lua_Debug ar = {};
    int got_ar = lua_getstack(L, 1, &ar);
    if (got_ar == 1) {
        lua_getinfo(L, "lnS", &ar);
        // l -> ar.currentline
        // n -> ar.name, ar.namewhat
        // S -> ar.short_src (among others)
    } else {
        LOG_FLOW("no debug information for caller stack");
        ar.currentline = -1;
    }
    if (!ar.name) { ar.name = "<lua>"; }

    if (file) { kmemcpy(file, ar.short_src, LUA_IDSIZE); }
    if (func) { *func = ar.name; }
    if (line) { *line = ar.currentline; }
}
