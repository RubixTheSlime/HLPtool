#ifndef SEARCH_HLP_RANDOM_H
#define SEARCH_HLP_RANDOM_H
#include "../arg_global.h"
#include "../solver/hlp_solve.h"

uint64_t rand_uint64();

struct arg_settings_search_hlp_random {
    struct arg_settings_global* global;
    int trials;
    int group;
    int seed;
    struct arg_settings_solver_hex settings_solver_hex;
};

extern struct argp argp_search_hlp_random;

#endif

