#ifndef DBIN_SOLVE_H
#define DBIN_SOLVE_H
#include <stdint.h>
#include "../arg_global.h"


int dbin_solve_exact(uint32_t map, uint16_t* output_chain, int max_depth);
int dbin_solve(uint64_t map, uint16_t* output_chain, int max_depth);

uint64_t dbin_expand_exact(uint32_t input);
void dbin_print_solve(uint64_t map);

struct arg_settings_solver_dbin {
    struct arg_settings_global* global;
};

extern struct argp argp_solver_dbin;

#endif
