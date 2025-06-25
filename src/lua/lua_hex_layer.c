#include "lua_open.h"
#include <lua.h>
#include <lauxlib.h>


static int layer_of(lua_State *L) {
    hex_layer_t layer = 0;
    hex_layer_set_barrel(&layer, 0, clamp_hex(lua_tointeger(L, 1)));
    hex_layer_set_sub(&layer, 0, lua_toboolean(L, 2));
    hex_layer_set_neg(&layer, 0, lua_toboolean(L, 3));
    hex_layer_set_barrel(&layer, 1, clamp_hex(lua_tointeger(L, 4)));
    hex_layer_set_sub(&layer, 1, lua_toboolean(L, 5));
    hex_layer_set_neg(&layer, 1, lua_toboolean(L, 6));
    lua_push_hex_layer(L, layer);
    return 1;
}

static int layer_classic_of(lua_State *L) {
    hex_layer_t layer = 0;
    hex_layer_set_barrel(&layer, 0, clamp_hex(lua_tointeger(L, 1)));
    hex_layer_set_sub(&layer, 0, lua_toboolean(L, 2));
    hex_layer_set_neg(&layer, 0, false);
    hex_layer_set_barrel(&layer, 1, clamp_hex(lua_tointeger(L, 3)));
    hex_layer_set_sub(&layer, 1, lua_toboolean(L, 4));
    hex_layer_set_neg(&layer, 1, true);
    lua_push_hex_layer(L, layer);
    return 1;
}

static int iter_layers(lua_State *L) {
    bool unique = lua_toboolean(L, 1);
    bool filtered = lua_gettop(L) >= 2;
    if (filtered) {
        luaL_checktype(L, 2, LUA_TFUNCTION);
    }
    hex_layer_iter_t iter;
    iter.type = unique ? HEX_LAYER_ITER_UNIQUE : HEX_LAYER_ITER_ALL;
    iter.filtered = filtered;
    iter.index = unique ? hex_layer_unique_count() : HEX_LAYER_COUNT;
    iter.data.empty = 0;
    lua_push_hex_layer_iter(L, iter);
    if (filtered) {
        lua_pushvalue(L, 2);
        lua_setiuservalue(L, -2, 2);
    }
    return 1;
}


static int get_barrel(lua_State *L) {
    hex_layer_t *layer = lua_get_hex_layer(L, 1, true);
    int half = lua_tointeger(L, 2);
    lua_pushinteger(L, hex_layer_get_barrel(*layer, half - 1));
    return 1;
}

static int set_barrel(lua_State *L) {
    hex_layer_t *layer = lua_get_hex_layer(L, 1, true);
    int half = lua_tointeger(L, 2);
    int barrel = lua_tointeger(L, 3);
    *layer = hex_layer_with_barrel(*layer, half - 1, barrel);
    return 0;
}

static int with_barrel(lua_State *L) {
    hex_layer_t *layer = lua_get_hex_layer(L, 1, true);
    int half = lua_tointeger(L, 2);
    int barrel = lua_tointeger(L, 3);
    lua_push_hex_layer(L, hex_layer_with_barrel(*layer, half - 1, barrel));
    return 1;
}

static int get_sub(lua_State *L) {
    hex_layer_t *layer = lua_get_hex_layer(L, 1, true);
    int half = lua_tointeger(L, 2);
    lua_pushboolean(L, hex_layer_get_sub(*layer, half - 1));
    return 1;
}

static int set_sub(lua_State *L) {
    hex_layer_t *layer = lua_get_hex_layer(L, 1, true);
    int half = lua_tointeger(L, 2);
    bool sub = lua_toboolean(L, 3);
    *layer = hex_layer_with_sub(*layer, half - 1, sub);
    return 0;
}

static int with_sub(lua_State *L) {
    hex_layer_t *layer = lua_get_hex_layer(L, 1, true);
    int half = lua_tointeger(L, 2);
    bool sub = lua_toboolean(L, 3);
    lua_push_hex_layer(L, hex_layer_with_sub(*layer, half - 1, sub));
    return 1;
}

static int get_neg(lua_State *L) {
    hex_layer_t *layer = lua_get_hex_layer(L, 1, true);
    int half = lua_tointeger(L, 2);
    lua_pushboolean(L, hex_layer_get_neg(*layer, half - 1));
    return 1;
}

static int set_neg(lua_State *L) {
    hex_layer_t *layer = lua_get_hex_layer(L, 1, true);
    int half = lua_tointeger(L, 2);
    bool side = lua_toboolean(L, 3);
    *layer = hex_layer_with_neg(*layer, half - 1, side);
    return 0;
}

static int with_neg(lua_State *L) {
    hex_layer_t *layer = lua_get_hex_layer(L, 1, true);
    int half = lua_tointeger(L, 2);
    bool side = lua_toboolean(L, 3);
    lua_push_hex_layer(L, hex_layer_with_neg(*layer, half - 1, side));
    return 1;
}

static int is_classic(lua_State *L) {
    hex_layer_t *layer = lua_get_hex_layer(L, 1, true);
    int variant = lua_tointeger(L, 2);
    lua_pushboolean(L, hex_layer_is_classic(*layer, variant));
    return 1;
}

static int get_alts(lua_State *L) {
    hex_layer_t layer = *lua_get_hex_layer(L, 1, true);
    bool exclude_self = lua_toboolean(L, 2);
    bool filtered = lua_gettop(L) >= 3;
    if (filtered) {
        luaL_checktype(L, 3, LUA_TFUNCTION);
    }
    const hex_layer_data_t *data = hex_layer_get_data(layer);
    hex_layer_iter_t iter;
    iter.type = HEX_LAYER_ITER_LIST;
    iter.filtered = filtered;
    iter.index = data->config_count;
    iter.data.alts.list = data->configs;
    iter.data.alts.exclude = exclude_self ? layer : hex_layer_unique_count();
    lua_push_hex_layer_iter(L, iter);
    if (filtered) {
        lua_pushvalue(L, 3);
        lua_setiuservalue(L, -1, 2);
    }
    return 1;
}

static int get_fn(lua_State *L) {
    hex_layer_t *layer = lua_get_hex_layer(L, 1, true);
    lua_push_hex_fn(L, hex_layer_get_data(*layer)->fn);
    return 1;
}

static int eq(lua_State *L) {
    hex_layer_t *a = lua_get_hex_layer(L, 1, false);
    hex_layer_t *b = lua_get_hex_layer(L, 2, false);
    lua_pushboolean(L, a != NULL && b != NULL && *a == *b);
    return 1;
}

static int tostring(lua_State *L) {
    hex_layer_t hex_layer = *lua_get_hex_layer(L, 1, true);
    char str[32];
    char *p = str;
    p += hex_layer_sprint(p, hex_layer);
    *p = '\0';
    lua_pushstring(L, str);
    return 1;
}

static const luaL_Reg functions[] = {
    {"of_classic", layer_classic_of},
    {"of", layer_of},
    {"iter", iter_layers},
    {NULL, NULL}
};

static const luaL_Reg methods[] = {
    {"get_barrel", get_barrel},
    {"set_barrel", set_barrel},
    {"with_barrel", with_barrel},
    {"get_sub", get_sub},
    {"set_sub", set_sub},
    {"with_sub", with_sub},
    {"get_neg", get_neg},
    {"set_neg", set_neg},
    {"with_neg", with_neg},
    {"is_classic", is_classic},
    {"alts", get_alts},
    {"get_fn", get_fn},
    {"__eq", eq},
    {"__tostring", tostring},
    {NULL, NULL}
};

const struct hlpt_lua_object_definition hex_layer_object_definition = {"hex_layer", functions, methods, NULL};

static int iter_call(lua_State *L) {
    hex_layer_iter_t *iter = lua_get_hex_layer_iter(L, 1, true);
    while (iter->index > 0) {
        iter->index--;
        uint16_t index = iter->index;
        uint16_t limit = 1;
        switch (iter->type) {
            case HEX_LAYER_ITER_ALL:
            case HEX_LAYER_ITER_LIST:
                limit = 1;
                break;
            case HEX_LAYER_ITER_UNIQUE:
                limit = hex_layer_get_data_by_id(index)->config_count;
                break;
        }
        for (uint16_t i = 0; i < limit; i++) {
            hex_layer_t hex_layer = 0;;
            switch (iter->type) {
                case HEX_LAYER_ITER_ALL:
                    hex_layer = index;
                    break;
                case HEX_LAYER_ITER_UNIQUE:
                    hex_layer = hex_layer_get_data_by_id(index)->configs[i];
                    break;
                case HEX_LAYER_ITER_LIST:
                    hex_layer = iter->data.alts.list[index];
                    if (hex_layer == iter->data.alts.exclude) hex_layer = HEX_LAYER_COUNT;
                    break;
            }
            if (hex_layer >= HEX_LAYER_COUNT) continue;
            lua_push_hex_layer(L, hex_layer);
            if (!iter->filtered) return 1;
            lua_getiuservalue(L, 1, 2);
            lua_pushvalue(L, -2);
            lua_call(L, 1, 1);
            bool success = lua_toboolean(L, -1);
            lua_pop(L, 1);
            if (success) return 1;
        }
    }
    return 0;
}

static const luaL_Reg iter_methods[] = {
    {"__call", iter_call},
    {NULL, NULL}
};

const struct hlpt_lua_object_definition hex_layer_iter_object_definition = {"hex_layer_iter", NULL, iter_methods, NULL};
