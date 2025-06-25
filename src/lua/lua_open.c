#include "lua_open.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdio.h>

#define STORE_FN(name) name##_store(x, name)
#define STORE_PTR(name) *x = name

#define LUA_GS(name, storage, uv) \
name##_t * lua_get_##name(lua_State *L, int idx, bool required) { \
    void *ud = luaL_checkudata(L, idx, "hlp." #name); \
    luaL_argcheck(L, !required || ud != NULL, idx, "Expected hlp." #name " object"); \
    return ud; \
} \
void lua_push_##name(lua_State *L, name##_t name) { \
    name##_t *x = lua_newuserdatauv(L, sizeof(name##_t), uv); \
    luaL_getmetatable(L, "hlp." #name); \
    lua_setmetatable(L, -2); \
    storage(name); \
}

LUA_HLPT_ALL(LUA_GS)

#undef STORE_FN
#undef STORE_PTR
#undef LUA_GS
#define LUA_GS(name, a, b) &name##_object_definition,


static const struct hlpt_lua_object_definition *object_definitions[] = {
    LUA_HLPT_ALL(LUA_GS)
    NULL
};

#undef LUA_GS

static const struct luaL_Reg main[] = {
    {NULL, NULL}
};

void hlpt_push_main_table(lua_State *L) {
    luaL_newlib(L, main);
    char name[1024];
    for (const struct hlpt_lua_object_definition **o = object_definitions; *o != NULL; o++) {
        const struct hlpt_lua_object_definition *def = *o;
        sprintf(name, "hlp.%s", def->name);
        luaL_newmetatable(L, name);
        lua_pushstring(L, "__index");
        lua_pushvalue(L, -2);
        lua_settable(L, -3);
        luaL_setfuncs(L, def->methods, 0);
        lua_pop(L, 1);
        if (def->functions) {
            lua_newtable(L);
            luaL_setfuncs(L, def->functions, 0);
            lua_setfield(L, -2, def->name);
        } else if (def->instance) {
            def->instance(L);
            lua_setfield(L, -2, def->name);
        }
    }
}
