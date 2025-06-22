#include "lua_open.h"
#include <lua.h>
#include <lauxlib.h>

static int new_of(lua_State *L) {
    bool is_table = lua_istable(L, 1);
    if (!is_table && !lua_isfunction(L, 1)) {
        return luaL_error(L, "Expected a table or function");
    }
    fn_set_t fn_set = fn_set_new_empty();
    hex_set_t hex_set;
    for (int i = 0; i < 16; i++) {
        if (is_table) {
            lua_pushinteger(L, i);
            lua_rawget(L, 1);
            hex_set = *lua_get_hex_set(L, -1);
            lua_pop(L, 1);
        } else {
            for (int j = 0; j < 16; j++) {
                lua_pushvalue(L, 1);
                lua_pushinteger(L, i);
                lua_pushinteger(L, j);
                lua_call(L, 2, 1);
                bool value = lua_toboolean(L, -1);
                lua_pop(L, 1);
                hex_set_set(&hex_set, j, value);
            }
        }
        fn_set_set_io(&fn_set, i, hex_set);
    }
    lua_push_fn_set(L, fn_set);

    return 1;
}

static int empty(lua_State *L) {
    lua_push_fn_set(L, fn_set_new_empty());
    return 1;
}

static int full(lua_State *L) {
    lua_push_fn_set(L, fn_set_new_full());
    return 1;
}

static int identity(lua_State *L) {
    lua_push_fn_set(L, fn_set_new_identity());
    return 1;
}

static int single(lua_State *L) {
    hex_fn_t hex_fn = hex_fn_load(lua_get_hex_fn(L, 1));
    lua_push_fn_set(L, fn_set_new_containing(hex_fn));
    return 1;
}

static int get_bit(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    int input = lua_tointeger(L, 2);
    int output = lua_tointeger(L, 3);
    lua_pushboolean(L, fn_set_get_bit(fn_set, input, output));
    return 1;
}

static int set_bit(lua_State *L) {
    fn_set_t *fn_set = lua_get_fn_set(L, 1);
    int input = lua_tointeger(L, 2);
    int output = lua_tointeger(L, 3);
    int value = lua_toboolean(L, 4);
    fn_set_set_bit(fn_set, input, output, value);
    return 0;
}

static int with_bit(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    int input = lua_tointeger(L, 2);
    int output = lua_tointeger(L, 3);
    int value = lua_toboolean(L, 4);
    lua_push_fn_set(L, fn_set_with_bit(fn_set, input, output, value));
    return 1;
}

static int get_io(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    int input = lua_tointeger(L, 2);
    lua_push_hex_set(L, fn_set_get_io(fn_set, input));
    return 1;
}

static int set_io(lua_State *L) {
    fn_set_t *fn_set = lua_get_fn_set(L, 1);
    int input = lua_tointeger(L, 2);
    hex_set_t value = *lua_get_hex_set(L, 3);
    fn_set_set_io(fn_set, input, value);
    return 0;
}

static int with_io(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    int input = lua_tointeger(L, 2);
    hex_set_t value = *lua_get_hex_set(L, 3);
    lua_push_fn_set(L, fn_set_with_io(fn_set, input, value));
    return 1;
}

static int get_oi(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    int input = lua_tointeger(L, 2);
    lua_push_hex_set(L, fn_set_get_oi(fn_set, input));
    return 1;
}

static int set_oi(lua_State *L) {
    fn_set_t *fn_set = lua_get_fn_set(L, 1);
    int input = lua_tointeger(L, 2);
    hex_set_t value = *lua_get_hex_set(L, 3);
    fn_set_set_oi(fn_set, input, value);
    return 0;
}

static int with_oi(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    int input = lua_tointeger(L, 2);
    hex_set_t value = *lua_get_hex_set(L, 3);
    lua_push_fn_set(L, fn_set_with_oi(fn_set, input, value));
    return 1;
}

static int is_super_of(lua_State *L) {
    fn_set_t a = fn_set_load(lua_get_fn_set(L, 1));
    fn_set_t b = fn_set_load(lua_get_fn_set(L, 2));
    lua_pushboolean(L, fn_set_is_super(a, b));
    return 1;
}

static int contains_identity(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    lua_pushboolean(L, fn_set_contains_identity(fn_set));
    return 1;
}

static int contains(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    hex_fn_t hex_fn = hex_fn_load(lua_get_hex_fn(L, 2));
    lua_pushboolean(L, fn_set_contains_fn(fn_set, hex_fn));
    return 1;
}

static int is_empty(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    lua_pushboolean(L, fn_set_is_equal(fn_set, fn_set_new_empty()));
    return 1;
}

static int is_full(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    lua_pushboolean(L, fn_set_is_equal(fn_set, fn_set_new_full()));
    return 1;
}

static int pre_mul(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    hex_fn_t hex_fn = hex_fn_load(lua_get_hex_fn(L, 2));
    lua_push_fn_set(L, fn_set_pre_mul_fn(fn_set, hex_fn));
    return 1;
}

static int pre_div(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    hex_fn_t hex_fn = hex_fn_load(lua_get_hex_fn(L, 2));
    lua_push_fn_set(L, fn_set_pre_div_fn(fn_set, hex_fn));
    return 1;
}

static int post_mul(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    hex_fn_t hex_fn = hex_fn_load(lua_get_hex_fn(L, 2));
    lua_push_fn_set(L, fn_set_post_mul_fn(fn_set, hex_fn));
    return 1;
}

static int post_div(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    hex_fn_t hex_fn = hex_fn_load(lua_get_hex_fn(L, 2));
    lua_push_fn_set(L, fn_set_post_div_fn(fn_set, hex_fn));
    return 1;
}

static int inverse(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    lua_push_fn_set(L, fn_set_inverse(fn_set));
    return 1;
}

static int clone(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    lua_push_fn_set(L, fn_set);
    return 1;
}

static int eq(lua_State *L) {
    fn_set_t a = fn_set_load(lua_get_fn_set(L, 1));
    fn_set_t b = fn_set_load(lua_get_fn_set(L, 2));
    lua_pushboolean(L, fn_set_is_equal(a, b));
    return 1;
}

static int tostring(lua_State *L) {
    fn_set_t fn_set = fn_set_load(lua_get_fn_set(L, 1));
    char str[1024];
    char *p = str;
    p += fn_set_sprint(p, fn_set);
    *p = '\0';
    lua_pushstring(L, str);
    return 1;
}

static const luaL_Reg functions[] = {
    {"empty", empty},
    {"full", full},
    {"identity", identity},
    {"single", single},
    {"new", new_of},
    {NULL, NULL}
};

static const luaL_Reg methods[] = {
    {"get", get_bit},
    {"set", set_bit},
    {"with", with_bit},
    {"get_io", get_io},
    {"set_io", set_io},
    {"with_io", with_io},
    {"get_oi", get_oi},
    {"set_oi", set_oi},
    {"with_oi", with_oi},
    {"is_super", is_super_of},
    {"contains_identity", contains_identity},
    {"contains", contains},
    {"is_empty", is_empty},
    {"is_full", is_full},
    {"pre_mul", pre_mul},
    {"post_mul", post_mul},
    {"pre_div", pre_div},
    {"post_div", post_div},
    {"inverse", inverse},
    {"clone", clone},
    {"__eq", eq},
    {"__tostring", tostring},
    {NULL, NULL}
};

const struct hlpt_lua_object fn_set_object = {"fn_set", functions, methods};
