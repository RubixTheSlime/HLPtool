#ifndef COMMAND_DBIN_H
#define COMMAND_DBIN_H
#include <stdint.h>
#include "../arg_global.h"
#include "../solver/dbin_solve.h"

struct arg_settings_command_dbin {
    struct arg_settings_global* global;
    uint64_t bits;
    int bit_count;
    int transpose, reverse;
    struct arg_settings_solver_dbin settings_solver_dbin;
};

extern struct argp argp_command_dbin;

#endif

