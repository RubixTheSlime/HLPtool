#ifndef LUA_OPEN_H
#define LUA_OPEN_H
#include <lua.h>
#include "../solver/hex_fn.h"
#include "../solver/hex_layer.h"

enum HEX_LAYER_ITER_TYPE {
    HEX_LAYER_ITER_ALL,
    HEX_LAYER_ITER_UNIQUE,
    HEX_LAYER_ITER_LIST,
};

typedef struct {
    enum HEX_LAYER_ITER_TYPE type;
    bool filtered;
    uint16_t index;
    union {
        struct {
            const hex_layer_t *list;
            hex_layer_t exclude;
        } alts;
        int empty;
    } data;
} hex_layer_iter_t;

struct hlpt_lua_object_definition {
    const char *name;
    const struct luaL_Reg *functions;
    const struct luaL_Reg *methods;
    const lua_CFunction instance;
};

#define LUA_HLPT_ALL(mac) \
mac(hex_fn, STORE_FN, 1) \
mac(fn_set, STORE_FN, 1) \
mac(hex_set, STORE_PTR, 1) \
mac(hex_layer, STORE_PTR, 1) \
mac(hex_layer_iter, STORE_PTR, 2) \


#define LUA_GS(name, a, b) \
extern name##_t *lua_get_##name(lua_State *L, int idx, bool required); \
extern void lua_push_##name(lua_State *L, name##_t name); \
extern const struct hlpt_lua_object_definition name##_object_definition;

LUA_HLPT_ALL(LUA_GS)
#undef LUA_GS

extern void hlpt_push_main_table(lua_State *L);

#endif //LUA_OPEN_H
