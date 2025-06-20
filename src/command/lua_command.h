#ifndef COMMAND_LUA_H
#define COMMAND_LUA_H
#include "../arg_global.h"
#include "../solver/hlp_solve.h"

struct arg_settings_lua {
    struct arg_settings_global* global;
};

struct arg_settings_command_lua {
    struct arg_settings_global* global;
    char* filename;
    struct arg_settings_lua settings_lua;
};

extern struct argp argp_command_lua;

#endif
