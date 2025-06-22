#ifndef LUA_OPEN_H
#define LUA_OPEN_H
#include <lua.h>
#include "../solver/hex_fn.h"


struct hlpt_lua_object {
    const char *name;
    const struct luaL_Reg *functions;
    const struct luaL_Reg *methods;
};

extern hex_fn_t* lua_get_hex_fn(lua_State* L, int idx);
extern hex_set_t* lua_get_hex_set(lua_State* L, int idx);
extern fn_set_t* lua_get_fn_set(lua_State* L, int idx);

extern void lua_push_hex_fn(lua_State* L, hex_fn_t hex_fn);
extern void lua_push_hex_set(lua_State* L, hex_set_t hex_set);
extern void lua_push_fn_set(lua_State* L, fn_set_t fn_set);

extern const struct hlpt_lua_object hex_fn_object;
extern const struct hlpt_lua_object hex_set_object;
extern const struct hlpt_lua_object fn_set_object;

extern void hlpt_push_main_table(lua_State *L);

#endif //LUA_OPEN_H
