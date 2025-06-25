#include "lua_open.h"
#include <lua.h>
#include <lauxlib.h>

static int from_values(lua_State* L) {
    hex_set_t hex_set = hex_set_empty();
    for (int i = 0;; i++) {
        lua_rawgeti(L, 1, i + 1);
        if (lua_isnil(L, -1)) {
            break;
        }
        int x = lua_tointeger(L, -1);
        hex_set_set(&hex_set, x, 1);
    }
    lua_push_hex_set(L, hex_set);
    return 1;
}

static int from_keys(lua_State* L) {
    bool is_table = lua_istable(L, 1);
    if (!is_table && !lua_isfunction(L, 1)) {
        return luaL_error(L, "Expected a table or function");
    }
    hex_set_t hex_set = hex_set_empty();
    for (int i = 0; i < 16; i++) {
        if (is_table) {
            lua_pushinteger(L, i);
            lua_rawget(L, 1);
        } else {
            lua_pushvalue(L, 1);
            lua_pushinteger(L, i);
            lua_call(L, 1, 1);
        }
        bool j = lua_toboolean(L, -1);
        lua_pop(L, 1);
        hex_set_set(&hex_set, i, j);
    }
    lua_push_hex_set(L, hex_set);
    return 1;
}

static int to_values(lua_State* L) {
    hex_set_t hex_set = *lua_get_hex_set(L, 1, true);
    lua_newtable(L);
    int index = 1;
    for (int i = 0; i < 16; i++) {
        if (!hex_set_get(hex_set, i)) {
            continue;
        }
        lua_pushinteger(L, i);
        lua_rawseti(L, -2, index++);
    }
    return 1;
}

static int empty(lua_State* L) {
    lua_push_hex_set(L, hex_set_empty());
    return 1;
}

static int full(lua_State* L) {
    lua_push_hex_set(L, hex_set_full());
    return 1;
}

static int single(lua_State* L) {
    int index = lua_tointeger(L, 1);
    lua_push_hex_set(L, hex_set_with(hex_set_empty(), index, true));
    return 1;
}



static int to_keys(lua_State* L) {
    hex_set_t hex_set = *lua_get_hex_set(L, 1, true);
    lua_newtable(L);
    for (int i = 0; i < 16; i++) {
        if (!hex_set_get(hex_set, i)) {
            continue;
        }
        lua_pushinteger(L, i);
        lua_pushboolean(L, true);
        lua_settable(L, -3);
    }
    return 1;
}

static int get(lua_State *L) {
    hex_set_t *hex_set = lua_get_hex_set(L, 1, true);
    int i = lua_tointeger(L, 2);
    lua_pushboolean(L, hex_set_get(*hex_set, i));
    return 1;
}

static int add(lua_State *L) {
    hex_set_t *hex_set = lua_get_hex_set(L, 1, true);
    int i = lua_tointeger(L, 2);
    bool value = true;
    if (lua_gettop(L) > 2) {
        value = lua_toboolean(L, 3);
    }
    hex_set_set(hex_set, i, value);
    return 0;
}

static int mremove(lua_State *L) {
    hex_set_t *hex_set = lua_get_hex_set(L, 1, true);
    int i = lua_tointeger(L, 2);
    hex_set_set(hex_set, i, false);
    return 0;
}

static int with(lua_State *L) {
    hex_set_t hex_set = *lua_get_hex_set(L, 1, true);
    int i = lua_tointeger(L, 2);
    bool value = true;
    if (lua_gettop(L) > 2) {
        value = lua_toboolean(L, 3);
    }
    lua_push_hex_set(L, hex_set_with(hex_set, i, value));
    return 1;
}

static int without(lua_State *L) {
    hex_set_t hex_set = *lua_get_hex_set(L, 1, true);
    int i = lua_tointeger(L, 2);
    lua_push_hex_set(L, hex_set_with(hex_set, i, false));
    return 1;
}

static int is_empty(lua_State *L) {
    hex_set_t hex_set = *lua_get_hex_set(L, 1, true);
    lua_pushboolean(L, hex_set_is_empty(hex_set));
    return 1;
}

static int is_full(lua_State *L) {
    hex_set_t hex_set = *lua_get_hex_set(L, 1, true);
    lua_pushboolean(L, hex_set_is_full(hex_set));
    return 1;
}

static int munion(lua_State *L) {
    hex_set_t a = *lua_get_hex_set(L, 1, true);
    hex_set_t b = *lua_get_hex_set(L, 2, true);
    lua_push_hex_set(L, hex_set_union(a, b));
    return 1;
}

static int intersect(lua_State *L) {
    hex_set_t a = *lua_get_hex_set(L, 1, true);
    hex_set_t b = *lua_get_hex_set(L, 2, true);
    lua_push_hex_set(L, hex_set_intersect(a, b));
    return 1;
}

static int diff(lua_State *L) {
    hex_set_t a = *lua_get_hex_set(L, 1, true);
    hex_set_t b = *lua_get_hex_set(L, 2, true);
    lua_push_hex_set(L, hex_set_diff(a, b));
    return 1;
}

static int sym_diff(lua_State *L) {
    hex_set_t a = *lua_get_hex_set(L, 1, true);
    hex_set_t b = *lua_get_hex_set(L, 2, true);
    lua_push_hex_set(L, hex_set_sym_diff(a, b));
    return 1;
}

static int complement(lua_State *L) {
    hex_set_t hex_set = *lua_get_hex_set(L, 1, true);
    lua_push_hex_set(L, hex_set_complement(hex_set));
    return 1;
}

static int clone(lua_State *L) {
    hex_set_t hex_set = *lua_get_hex_set(L, 1, true);
    lua_push_hex_set(L, hex_set);
    return 1;
}

static int band(lua_State *L) {
    return intersect(L);
}

static int bor(lua_State *L) {
    return munion(L);
}

static int bxor(lua_State *L) {
    return sym_diff(L);
}

static int bnot(lua_State *L) {
    return complement(L);
}

static int sub(lua_State *L) {
    return diff(L);
}

static int len(lua_State *L) {
    hex_set_t hex_set = *lua_get_hex_set(L, 1, true);
    lua_pushinteger(L, hex_set_len(hex_set));
    return 1;
}

static int equals(lua_State *L) {
    hex_set_t *a = lua_get_hex_set(L, 1, false);
    hex_set_t *b = lua_get_hex_set(L, 2, false);
    lua_pushboolean(L, a != NULL && b != NULL && *a == *b);
    return 1;
}

static int tostring(lua_State *L) {
    hex_set_t hex_set = *lua_get_hex_set(L, 1, true);
    char str[256];
    char *p = str;
    p += hex_set_sprint(p, hex_set);
    *p = '\0';
    lua_pushstring(L, str);
    return 1;
}


static const luaL_Reg functions[] = {
    {"from_values", from_values},
    {"from_keys", from_keys},
    {"empty", empty},
    {"full", full},
    {"single", single},
    {NULL, NULL}
};

static const luaL_Reg methods[] = {
    {"contains", get},
    {"add", add},
    {"remove", mremove},
    {"with", with},
    {"without", without},
    {"is_empty", is_empty},
    {"is_full", is_full},
    {"to_values", to_values},
    {"to_keys", to_keys},
    {"union", munion},
    {"intersect", intersect},
    {"diff", diff},
    {"sym_diff", sym_diff},
    {"complement", complement},
    {"clone", clone},
    {"__band", band},
    {"__bor", bor},
    {"__bxor", bxor},
    {"__bnot", bnot},
    {"__sub", sub},
    {"__len", len},
    {"__eq", equals},
    {"__tostring", tostring},
    {NULL, NULL}
};

const struct hlpt_lua_object_definition hex_set_object_definition = {"hex_set", functions, methods, NULL};
