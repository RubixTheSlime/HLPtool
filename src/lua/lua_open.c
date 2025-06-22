#include "lua_open.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdio.h>


hex_fn_t* lua_get_hex_fn(lua_State* L, int idx) {
    void *ud = luaL_checkudata(L, idx, "hlp.hex_fn");
    luaL_argcheck(L, ud != NULL, idx, "Expected hlp.hex_fn object");
    return ud;
}

hex_set_t * lua_get_hex_set(lua_State *L, int idx) {
    void *ud = luaL_checkudata(L, idx, "hlp.hex_set");
    luaL_argcheck(L, ud != NULL, idx, "Expected hlp.hex_set object");
    return ud;
}

fn_set_t * lua_get_fn_set(lua_State *L, int idx) {
    void *ud = luaL_checkudata(L, idx, "hlp.fn_set");
    luaL_argcheck(L, ud != NULL, idx, "Expected hlp.fn_set object");
    return ud;
}

void lua_push_hex_fn(lua_State* L, hex_fn_t hex_fn) {
    hex_fn_t* x = lua_newuserdata(L, sizeof(hex_fn_t));
    luaL_getmetatable(L, "hlp.hex_fn");
    lua_setmetatable(L, -2);
    hex_fn_store(x, hex_fn);
}

void lua_push_hex_set(lua_State *L, hex_set_t hex_set) {
    hex_set_t* x = lua_newuserdata(L, sizeof(hex_set_t));
    luaL_getmetatable(L, "hlp.hex_set");
    lua_setmetatable(L, -2);
    *x = hex_set;
}

void lua_push_fn_set(lua_State *L, fn_set_t fn_set) {
    fn_set_t* x = lua_newuserdata(L, sizeof(fn_set_t));
    luaL_getmetatable(L, "hlp.fn_set");
    lua_setmetatable(L, -2);
    fn_set_store(x, fn_set);
}

static const struct hlpt_lua_object *objects[] = {
    &hex_fn_object,
    &hex_set_object,
    &fn_set_object,
    NULL
};

static const struct luaL_Reg main[] = {
    {NULL, NULL}
};

void hlpt_push_main_table(lua_State *L) {
    luaL_newlib(L, main);
    char name[256];
    for (const struct hlpt_lua_object **o = objects; *o != NULL; o++) {
        const struct hlpt_lua_object *def = *o;
        sprintf(name, "hlp.%s", def->name);
        luaL_newmetatable(L, name);
        lua_pushstring(L, "__index");
        lua_pushvalue(L, -2);
        lua_settable(L, -3);
        luaL_setfuncs(L, def->methods, 0);
        lua_pop(L, 1);
        lua_newtable(L);
        luaL_setfuncs(L, def->functions, 0);
        lua_setfield(L, -2, def->name);
    }
}
