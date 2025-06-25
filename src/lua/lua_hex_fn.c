#include "lua_open.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

static int identity(lua_State *L) {
    lua_push_hex_fn(L, hex_fn_identity());
    return 1;
}

static int from_lua_value(lua_State *L) {
    bool is_table = lua_istable(L, 1);
    if (!is_table && !lua_isfunction(L, 1)) {
        return luaL_error(L, "Expected a table or function");
    }
    hex_fn_t fn = hex_fn_identity();
    for (int i = 0; i < 16; i++) {
        if (is_table) {
            lua_pushinteger(L, i);
            lua_rawget(L, 1);
        } else {
            lua_pushvalue(L, 1);
            lua_pushinteger(L, i);
            lua_call(L, 1, 1);
        }
        int j = clamp_hex(lua_tointeger(L, -1));
        lua_pop(L, 1);
        hex_fn_set(&fn, i, j);
    }
    lua_push_hex_fn(L, fn);
    return 1;
}

static int of_constant(lua_State *L) {
    int k = luaL_checkinteger(L, 1);
    lua_push_hex_fn(L, hex_fn_of_constant(k));
    return 1;
}

static int compose_inner(lua_State *L, int idx1, int idx2) {
    hex_fn_t *a = lua_get_hex_fn(L, idx1, true);
    hex_fn_t *b = lua_get_hex_fn(L, idx2, true);
    lua_push_hex_fn(L, hex_fn_compose(hex_fn_load(a), hex_fn_load(b)));
    return 1;
}

static int compose(lua_State *L) {
    return compose_inner(L, 1, 2);
}

static int and_then(lua_State *L) {
    return compose_inner(L, 2, 1);
}

static int get(lua_State *L) {
    hex_fn_t *fn = lua_get_hex_fn(L, 1, true);
    int i = clamp_hex(lua_tointeger(L, 2));
    int res = hex_fn_get(hex_fn_load(fn), i);
    lua_pushinteger(L, res);
    return 1;
}

static int call(lua_State *L) {
    return get(L);
}

static int out_set(lua_State *L) {
    hex_fn_t *fn = lua_get_hex_fn(L, 1, true);
    lua_push_hex_set(L, hex_fn_out_set(hex_fn_load(fn)));
    return 1;
}

static int clone(lua_State *L) {
    hex_fn_t *fn = lua_get_hex_fn(L, 1, true);
    lua_push_hex_fn(L, hex_fn_load(fn));
    return 1;
}

static int set(lua_State *L) {
    hex_fn_t *fn = lua_get_hex_fn(L, 1, true);
    int i = clamp_hex(lua_tointeger(L, 2));
    int j = clamp_hex(lua_tointeger(L, 3));
    hex_fn_set(fn, i, j);
    return 0;
}

static int with(lua_State *L) {
    hex_fn_t *fn = lua_get_hex_fn(L, 1, true);
    int i = clamp_hex(lua_tointeger(L, 2));
    int j = clamp_hex(lua_tointeger(L, 3));
    lua_push_hex_fn(L, hex_fn_with(hex_fn_load(fn), i, j));
    return 1;
}

static int is_identity(lua_State *L) {
    hex_fn_t *fn = lua_get_hex_fn(L, 1, true);
    lua_pushboolean(L, hex_fn_is_identity(hex_fn_load(fn)));
    return 1;
}

static int is_constant(lua_State *L) {
    hex_fn_t *fn = lua_get_hex_fn(L, 1, true);
    int value = hex_fn_is_constant(hex_fn_load(fn));
    if (value == -1 || lua_gettop(L) < 2) {
        lua_pushboolean(L, value != -1);
    } else {
        int target = clamp_hex(lua_tointeger(L, 2));
        lua_pushboolean(L, target == value);
    }
    return 1;
}

static int equals(lua_State *L) {
    hex_fn_t *a = lua_get_hex_fn(L, 1, false);
    hex_fn_t *b = lua_get_hex_fn(L, 2, false);
    lua_pushboolean(L, a != NULL && b != NULL && hex_fn_is_equal(hex_fn_load(a), hex_fn_load(b)));
    return 1;
}

static int tostring(lua_State *L) {
    hex_fn_t hex_fn = hex_fn_load(lua_get_hex_fn(L, 1, true));
    char str[256];
    char *p = str;
    p += hex_fn_sprint(p, hex_fn);
    *p = '\0';
    lua_pushstring(L, str);
    return 1;
}

static const luaL_Reg functions[] = {
    {"new", from_lua_value},
    {"identity", identity},
    {"constant", of_constant},
    {NULL, NULL}
};

static const luaL_Reg methods[] = {
    {"compose", compose},
    {"and_then", and_then},
    {"with", with},
    {"is_identity", is_identity},
    {"is_constant", is_constant},
    {"get", get},
    {"set", set},
    {"out_set", out_set},
    {"clone", clone},
    {"__call", call},
    {"__eq", equals},
    {"__tostring", tostring},
    {NULL, NULL}
};

const struct hlpt_lua_object_definition hex_fn_object_definition = {"hex_fn", functions, methods, NULL};
