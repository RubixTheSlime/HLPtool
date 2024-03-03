#ifndef SEARCH_DBIN_RANDOM_H
#define SEARCH_DBIN_RANDOM_H
#include "../arg_global.h"
#include "../solver/dbin_solve.h"

struct arg_settings_search_dbin_random {
    struct arg_settings_global* global;
    int trials;
    int group;
    int seed;
    struct arg_settings_solver_dbin settings_solver_dbin;
};

extern struct argp argp_search_dbin_random;

#endif

