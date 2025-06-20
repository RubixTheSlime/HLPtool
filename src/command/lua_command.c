#include "lua_command.h"
#include <stdio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

int lua_chain_to_str(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    enum LAYER_NOTATION notation = get_layer_notation_by_name(lua_tostring(L, 2));
    uint16_t *chain = NULL;
    int length = lua2c_chain(L, 1, &chain);
    char *str = chain_to_str(chain, length, notation);
    lua_pushstring(L, str);
    free(str);
    free(chain);
    return 1;
}

int lua_eval_chain(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    bool initial = lua_gettop(L) > 1;
    if (initial) luaL_checktype(L, 2, LUA_TTABLE);
    uint16_t *chain = NULL;
    int length = lua2c_chain(L, 1, &chain);
    uint64_t map = 0;
    if (initial) {
        for (int i = 0; i < 16; i++) {
            lua_pushinteger(L, i);
            lua_rawget(L, 2);

            int j = lua_tointeger(L, -1);
            j = j < 0 ? 0 : j > 15 ? 15 : j;
            map = (map << 4) | j;

            lua_pop(L, 1);
        }
    } else {
        map = 0x0123456789ABCDEF;
    }
    map = apply_hex_chain(map, chain, length);
    free(chain);
    lua_newtable(L);
    for (int i = 0; i < 16; i++) {
        lua_pushinteger(L, i);
        lua_pushinteger(L, (int) ((map >> 60) & 15));
        lua_rawset(L, -3);
        map <<= 4;
    }

    return 1;
}

int lua_normalize_chain(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    uint16_t *chain = NULL;
    int length = lua2c_chain(L, 1, &chain);
    c2lua_chain(L, chain, length);
    free(chain);
    return 1;
}

static void exec_lua(const char *filename) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_newtable(L);
    lua_pushcfunction(L, lua_eval_chain);
    lua_setfield(L, -2, "eval");
    lua_pushcfunction(L, lua_chain_to_str);
    lua_setfield(L, -2, "chain_to_str");
    lua_pushcfunction(L, lua_normalize_chain);
    lua_setfield(L, -2, "normalize_chain");
    lua_pushcfunction(L, solve_lua);
    lua_setfield(L, -2, "solve");
    lua_setglobal(L, "hlp");
    int error = luaL_dofile(L, filename);
    if (error) {
        fprintf(stderr, "%s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_close(L);
}

static const char doc[] =
        "Execute a lua script";

static const struct argp_option options[] = {
    {0}
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arg_settings_command_lua *settings = state->input;
    switch (key) {
        case ARGP_KEY_ARG:
            settings->filename = arg;
            break;
        case ARGP_KEY_INIT:
            settings->filename = 0;
            settings->settings_lua.global = settings->global;
            state->child_inputs[0] = &settings->settings_lua;
            break;
        case ARGP_KEY_SUCCESS:
            exec_lua(settings->filename);
            break;
        case ARGP_KEY_NO_ARGS:
            argp_state_help(state, stderr, ARGP_HELP_USAGE | ARGP_HELP_SHORT_USAGE | ARGP_HELP_SEE);
            return 1;
    }
    return 0;
}

static struct argp_child argp_children[] = {
    {&argp_solver_hex, 0, 0, 0},
    {0}
};

struct argp argp_command_lua = {
    options,
    parse_opt,
    "FUNCTION",
    doc,
    argp_children
};
