#include "assert.h"
#include "heap.h"
#include "kbd.h"
#include "kinttypes.h"
#include "kprintf.h"
#include "kshell/kshcmd/kshcmd.h"
#include "kshell/kshell.h"
#include "kstring.h"
#include "log.h"
#include "mbi.h"
#include "memfun.h"
#include "panic.h"
#include "term.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#define CMD_BUF_SIZE     256
#define LUA_CMD_BUF_SIZE 256

static lua_State *g_kshell_lua;

// Command buffer.
static volatile char gp_cmd_buf[CMD_BUF_SIZE];
static volatile size_t g_cmd_buf_pos;

// Keyboard state.
static volatile bool gb_shifted;

// Input reading functions.
static char const *read_cmd(void);

// Command buffer functions.
static void buf_append(char ch);
static void buf_remove(void);
static volatile char const *buf_get_cmd(void);

// Keyboard-related functions.
static bool parse_kbd_event(kbd_event_t *p_event);
static void echo_key(uint8_t key);
static char char_from_key(uint8_t key);

static void prv_kshell_lua_init(const mbi_mod_t *mod);
static void prv_kshell_lua_deinit(void);
static int prv_kshell_lua_do_cmd(lua_State *L, const char *cmd);
static int prv_kshell_lua_exec_mod(lua_State *L, const char *s);
static int prv_kshell_lua_repl_expr(lua_State *L, const char *input);

void kshell(void) {
    for (;;) {
        kprintf("> ");
        char const *p_cmd = read_cmd();
        kshcmd_parse(p_cmd);
    }
}

void kshell_lua(void) {
    const mbi_mod_t *const kshell_mod = mbi_find_mod("kshell");
    if (!kshell_mod) { LOG_ERROR("no module named kshell.lua"); }

    prv_kshell_lua_init(kshell_mod);

    for (;;) {
        kprintf("Lua > ");
        char const *p_cmd = read_cmd();
        prv_kshell_lua_do_cmd(g_kshell_lua, p_cmd);
    }

    prv_kshell_lua_deinit();
}

/*
 * Synchronously reads a command from the user and returns it.
 *
 * NOTE: each call invalidates the pointer returned by the previous call.
 */
static char const *read_cmd(void) {
    bool b_enter = false;
    while (!b_enter) {
        kbd_event_t event;
        term_read_kbd_event(&event);
        b_enter = parse_kbd_event(&event);
    }

    return (char const *)buf_get_cmd();
}

static void buf_append(char ch) {
    if (g_cmd_buf_pos >= (CMD_BUF_SIZE - 1)) { return; }

    gp_cmd_buf[g_cmd_buf_pos] = ch;
    g_cmd_buf_pos++;
}

static void buf_remove(void) {
    if (g_cmd_buf_pos > 0) { g_cmd_buf_pos--; }
}

/*
 * Returns a NUL-terminated command string buffer and resets the buffer
 * position.
 */
static volatile char const *buf_get_cmd(void) {
    if (g_cmd_buf_pos >= CMD_BUF_SIZE) { PANIC("command buffer overflow"); }

    gp_cmd_buf[g_cmd_buf_pos] = 0;
    g_cmd_buf_pos = 0;
    return gp_cmd_buf;
}

static bool parse_kbd_event(kbd_event_t *p_event) {
    if (p_event->key == KEY_LSHIFT || p_event->key == KEY_RSHIFT) {
        gb_shifted = !p_event->b_released;
    }

    // Henceforth parse only 'key pressed' events.
    if (p_event->b_released) { return false; }

    if (p_event->key == KEY_BACKSPACE) {
        // Allow to delete as many characters, as there are in the command
        // buffer.
        if (g_cmd_buf_pos == 0) { return false; }

        term_acquire_mutex();
        size_t row = term_row();
        size_t col = term_col();

        if ((row == 0) && (col == 0)) {
            term_release_mutex();
            return false;
        }

        if (col > 0) {
            col--;
        } else if (row > 0) {
            row--;
            col = (term_width() - 1);
        }

        term_put_char_at(row, col, ' ');
        term_put_cursor_at(row, col);
        term_release_mutex();

        // Remove one char from the buffer.
        buf_remove();
    } else if (p_event->key == KEY_ENTER) {
        echo_key(p_event->key);
        return true;
    } else {
        // Print the key and add it to the buffer, if it's a char.
        echo_key(p_event->key);

        char ch = char_from_key(p_event->key);
        if (ch) { buf_append(ch); }
    }

    return false;
}

static void echo_key(uint8_t key) {
    char p_echoed[2] = {0};
    p_echoed[0] = char_from_key(key);
    if (p_echoed[0]) { kprintf("%s", p_echoed); }
}

static char char_from_key(uint8_t key) {
    switch (key) {
    case KEY_BACKTICK:
        return gb_shifted ? '~' : '`';
    case KEY_1:
        return gb_shifted ? '!' : '1';
    case KEY_2:
        return gb_shifted ? '@' : '2';
    case KEY_3:
        return gb_shifted ? '#' : '3';
    case KEY_4:
        return gb_shifted ? '$' : '4';
    case KEY_5:
        return gb_shifted ? '%' : '5';
    case KEY_6:
        return gb_shifted ? '^' : '6';
    case KEY_7:
        return gb_shifted ? '&' : '7';
    case KEY_8:
        return gb_shifted ? '*' : '8';
    case KEY_9:
        return gb_shifted ? '(' : '9';
    case KEY_0:
        return gb_shifted ? ')' : '0';
    case KEY_MINUS:
        return gb_shifted ? '_' : '-';
    case KEY_EQUALS:
        return gb_shifted ? '+' : '=';
    case KEY_Q:
        return gb_shifted ? 'Q' : 'q';
    case KEY_W:
        return gb_shifted ? 'W' : 'w';
    case KEY_E:
        return gb_shifted ? 'E' : 'e';
    case KEY_R:
        return gb_shifted ? 'R' : 'r';
    case KEY_T:
        return gb_shifted ? 'T' : 't';
    case KEY_Y:
        return gb_shifted ? 'Y' : 'y';
    case KEY_U:
        return gb_shifted ? 'U' : 'u';
    case KEY_I:
        return gb_shifted ? 'I' : 'i';
    case KEY_O:
        return gb_shifted ? 'O' : 'o';
    case KEY_P:
        return gb_shifted ? 'P' : 'p';
    case KEY_LBRACKET:
        return gb_shifted ? '{' : '[';
    case KEY_RBRACKET:
        return gb_shifted ? '}' : ']';
    case KEY_BACKSLASH:
        return gb_shifted ? '|' : '\\';
    case KEY_A:
        return gb_shifted ? 'A' : 'a';
    case KEY_S:
        return gb_shifted ? 'S' : 's';
    case KEY_D:
        return gb_shifted ? 'D' : 'd';
    case KEY_F:
        return gb_shifted ? 'F' : 'f';
    case KEY_G:
        return gb_shifted ? 'G' : 'g';
    case KEY_H:
        return gb_shifted ? 'H' : 'h';
    case KEY_J:
        return gb_shifted ? 'J' : 'j';
    case KEY_K:
        return gb_shifted ? 'K' : 'k';
    case KEY_L:
        return gb_shifted ? 'L' : 'l';
    case KEY_SEMICOLON:
        return gb_shifted ? ':' : ';';
    case KEY_APOSTROPHE:
        return gb_shifted ? '"' : '\'';
    case KEY_ENTER:
        return '\n';
    case KEY_Z:
        return gb_shifted ? 'Z' : 'z';
    case KEY_X:
        return gb_shifted ? 'X' : 'x';
    case KEY_C:
        return gb_shifted ? 'C' : 'c';
    case KEY_V:
        return gb_shifted ? 'V' : 'v';
    case KEY_B:
        return gb_shifted ? 'B' : 'b';
    case KEY_N:
        return gb_shifted ? 'N' : 'n';
    case KEY_M:
        return gb_shifted ? 'M' : 'm';
    case KEY_COMMA:
        return gb_shifted ? '<' : ',';
    case KEY_PERIOD:
        return gb_shifted ? '>' : '.';
    case KEY_SLASH:
        return gb_shifted ? '?' : '/';
    case KEY_SPACE:
        return ' ';

    default:
        return 0;
    }
}

static void prv_kshell_lua_init(const mbi_mod_t *mod) {
    g_kshell_lua = luaL_newstate();
    if (!g_kshell_lua) { PANIC("luaL_newstate failed"); }

    luaL_openlibs(g_kshell_lua);
    LOG_DEBUG("initialized lua");

    if (mod) {
        ASSERT(mod->mod_end > mod->mod_start);

        const char *const mod_start = (const char *)mod->mod_start;
        const uint32_t mod_size = mod->mod_end - mod->mod_start;
        LOG_DEBUG("copy module at %p len %" PRIu32 " to heap", mod_start,
                  mod->mod_end - mod->mod_start);

        char *const mod_str = heap_alloc(mod_size + 1);
        kmemcpy(mod_str, mod_start, mod_size);
        mod_str[mod_size] = 0;

        prv_kshell_lua_exec_mod(g_kshell_lua, mod_str);
    }
}

static void prv_kshell_lua_deinit(void) {
    lua_close(g_kshell_lua);
}

static int prv_kshell_lua_do_cmd(lua_State *L, const char *cmd) {
    int err;
    int base = lua_gettop(L);

    LOG_FLOW("parse and execute '%s' using Lua", cmd);
    LOG_FLOW("stack position at start = %d", base);

    int val_type = lua_getglobal(L, "S");
    if (val_type == LUA_TNIL) {
        LOG_ERROR("lua_getglobal returned nil for S, is the module loaded?");
        goto fail;
    }

    val_type = lua_getfield(L, -1, "do_cmd");
    if (val_type == LUA_TNIL) {
        LOG_ERROR("lua_getfield pushed nil for \"S.do_cmd\"");
        goto fail;
    }

    // Remove the table S from the stack.
    lua_remove(L, -2);

    // Argument for S.do_cmd.
    lua_pushstring(L, cmd);

    // Call S.do_cmd(cmd).
    err = lua_pcall(L, 1, LUA_MULTRET, 0);
    if (err != LUA_OK) {
        const char *const errstr = lua_tostring(L, -1);
        lua_pop(L, 1);
        LOG_ERROR("pcall(S.do_cmd) returned %d", err);
        LOG_ERROR("error string: %s", errstr);
        goto fail;
    }

    // Check the number of return values.
    int num_ret = lua_gettop(L) - base;
    LOG_FLOW("S.cmd returned %d values", num_ret);
    if (num_ret > 0) {
        for (int idx = 0; idx < num_ret; idx++) {
            int stack_idx = -(num_ret - idx);
            val_type = lua_type(L, stack_idx);
            const char *type_name = lua_typename(L, val_type);
            if (val_type == LUA_TSTRING || val_type == LUA_TNUMBER) {
                LOG_FLOW("  value %2d (stack %2d): [%s] %s", idx, stack_idx,
                         type_name, lua_tostring(L, stack_idx));
            } else {
                LOG_FLOW("  value %2d (stack %2d): %s", idx, stack_idx,
                         type_name);
            }
        }
    }
    if (num_ret == 2 && lua_type(L, -2) == LUA_TNIL) {
        // If the first returned value is nil, then the second must be an error
        // string.
        if (lua_type(L, -1) == LUA_TSTRING) {
            LOG_ERROR(lua_tostring(L, -1));
        } else {
            LOG_ERROR("S.do_cmd returned nil and %s, instead of nil and string",
                      lua_typename(L, lua_type(L, -1)));
        }
    }

    LOG_FLOW("stack position at end = %d, set to %d", lua_gettop(L), base);
    lua_settop(L, base);
    return 0;
fail:
    LOG_FLOW("stack position at end = %d, set to %d", lua_gettop(L), base);
    lua_settop(L, base);
    return 1;
}

static int prv_kshell_lua_exec_mod(lua_State *L, const char *s) {
    int err;

    LOG_FLOW("string len %zu", string_len(s));

    err = luaL_loadstring(L, s);
    if (err != 0) {
        const char *const errstr = lua_tostring(L, -1);
        lua_pop(L, 1);
        LOG_ERROR("pcall(luaL_loadstring) returned %d, len %zu", err,
                  string_len(s));
        LOG_ERROR("error string: %s", errstr);
        return err;
    }

    err = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (err != 0) {
        const char *const errstr = lua_tostring(L, -1);
        lua_pop(L, 1);
        LOG_ERROR("pcall returned %d", err);
        LOG_ERROR("error string: %s", errstr);
        return err;
    }

    return 0;
}

/**
 * Executes a Lua expression or statement.
 *
 * @param L     Lua state.
 * @param input Expression or statement in Lua.
 *
 * @returns `0` on success, `-1` on error.
 */
static int prv_kshell_lua_repl_expr(lua_State *L, const char *input) {
    int base = lua_gettop(L);

    char buf[512];

    // ---- 1. Try expression mode ----
    snprintf(buf, sizeof(buf), "return %s", input);

    int err = luaL_loadstring(L, buf);
    if (err == 0) { err = lua_pcall(L, 0, LUA_MULTRET, 0); }

    // ---- 2. Fallback: statement mode ----
    if (err != 0) {
        lua_pop(L, 1); // remove error

        err = luaL_loadstring(L, input);
        if (err != 0) {
            kprintf("%s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
            lua_settop(L, base);
            return -1;
        }

        err = lua_pcall(L, 0, LUA_MULTRET, 0);
        if (err != 0) {
            kprintf("%s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
            lua_settop(L, base);
            return -1;
        }
    }

    // ---- 3. Print results ----
    int n = lua_gettop(L) - base;

    for (int i = 1; i <= n; i++) {
        if (lua_isnil(L, i)) {
            kprintf("nil\n");
        } else {
            const char *s = luaL_tolstring(L, i, NULL);
            kprintf("%s\n", s);
            lua_pop(L, 1);
        }
    }

    lua_settop(L, base);
    return 0;
}
