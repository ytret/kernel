#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include "assert.h"
#include "heap.h"
#include "kinttypes.h"
#include "kprintf.h"
#include "kshell/kshcmd/ksh_lua.h"
#include "kshell/kshinput.h"
#include "kstring.h"
#include "log.h"
#include "mbi.h"
#include "memfun.h"
#include "panic.h"

#define LUA_CMD_BUF_SIZE 256
#define LUA_KOBJ_NAME    "K"

typedef struct {
    const char *name;
    const char *help;
    int (*f_handler)(lua_State *L);
} lua_shell_cmd_t;

static lua_State *g_kshlua_L;
static volatile bool g_kshlua_loop;

static void prv_kshlua_init(const mbi_mod_t *mod);
static void prv_kshlua_deinit(void);
static int prv_kshlua_exec_mod(lua_State *L, const char *s);
static void prv_kshlua_init_kobj(lua_State *L);

static int prv_kshlua_do_cmd(lua_State *L, const char *cmd);
static int prv_kshlua_repl_expr(lua_State *L, const char *input);

static int prv_kshlua_cmd_dotexit(lua_State *L);

static lua_shell_cmd_t g_kshell_lua_cmds[] = {
    {
        .name = ".exit",
        .help = "exit Lua kshell",
        .f_handler = prv_kshlua_cmd_dotexit,
    },
};

void ksh_lua(list_t *arg_list) {
    (void)arg_list;

    const mbi_mod_t *const kshell_mod = mbi_find_mod("kshell");
    if (!kshell_mod) { LOG_ERROR("no module named kshell.lua"); }

    prv_kshlua_init(kshell_mod);

    g_kshlua_loop = true;
    while (g_kshlua_loop) {
        kprintf("Lua > ");
        char const *p_cmd = kshinput_line();
        prv_kshlua_do_cmd(g_kshlua_L, p_cmd);
    }

    prv_kshlua_deinit();
}

static void prv_kshlua_init(const mbi_mod_t *mod) {
    g_kshlua_L = luaL_newstate();
    if (!g_kshlua_L) { PANIC("luaL_newstate failed"); }

    luaL_openlibs(g_kshlua_L);
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

        prv_kshlua_exec_mod(g_kshlua_L, mod_str);
    }

    prv_kshlua_init_kobj(g_kshlua_L);
}

static void prv_kshlua_deinit(void) {
    lua_close(g_kshlua_L);
}

static int prv_kshlua_exec_mod(lua_State *L, const char *s) {
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

static void prv_kshlua_init_kobj(lua_State *L) {
    lua_newtable(L); // KOBJ
    lua_newtable(L); // KOBJ.cmds

    for (size_t idx = 0;
         idx < sizeof(g_kshell_lua_cmds) / sizeof(lua_shell_cmd_t); idx++) {
        lua_shell_cmd_t *const cmd = &g_kshell_lua_cmds[idx];

        lua_newtable(L);

        lua_pushstring(L, cmd->help);
        lua_setfield(L, -2, "help");

        lua_pushcfunction(L, cmd->f_handler);
        lua_setfield(L, -2, "handler");

        lua_setfield(L, -2, cmd->name);
    }

    lua_setfield(L, -2, "cmds");
    lua_setglobal(L, LUA_KOBJ_NAME);
}

static int prv_kshlua_do_cmd(lua_State *L, const char *cmd) {
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

/**
 * Executes a Lua expression or statement.
 *
 * @param L     Lua state.
 * @param input Expression or statement in Lua.
 *
 * @returns `0` on success, `-1` on error.
 */
static int prv_kshlua_repl_expr(lua_State *L, const char *input) {
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

static int prv_kshlua_cmd_dotexit(lua_State *L) {
    (void)L;
    g_kshlua_loop = false;
    return 0;
}
