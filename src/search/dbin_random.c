#include "dbin_random.h"
#include "hlp_random.h"
#include <immintrin.h>
#include <time.h>
#include <stdlib.h>

static int verbosity;



static void random_search(int count, int group) {
    for (int i=0; i<count; i++) {
        uint32_t map = rand_uint64();
        dbin_print_solve(dbin_expand_exact(map));
    }
}

enum LONG_OPTION {
    LONG_OPTION_RANDOM_SEED = 1000,
    LONG_OPTION_RANDOM_SEARCH_GROUP
};

static const char doc[] =
"Solve random 2bin cases"
;

static const struct argp_option options[] = {
    { "trials", 'n', "N", 0, "Solve n random cases, default 1" },
    { "seed", LONG_OPTION_RANDOM_SEED, "SEED", 0, "Set the random seed, uses system clock if not set" },
    { 0 }
};

static error_t parse_opt(int key, char* arg, struct argp_state *state) {
    struct arg_settings_search_dbin_random* settings = state->input;
    switch (key) {
        case 'n':
            settings->trials = atoi(arg);
            break;
        case LONG_OPTION_RANDOM_SEED:
            settings->seed = atoi(arg);
            break;
        case ARGP_KEY_INIT:
            settings->trials = 1;
            settings->seed = clock();
            settings->settings_solver_dbin.global = settings->global;
            state->child_inputs[0] = &settings->settings_solver_dbin;
            break;
        case ARGP_KEY_SUCCESS:
            srand(settings->seed);
            verbosity = settings->global->verbosity;
            random_search(settings->trials, settings->group);
            return 1;
    }
    return 0;
}

static struct argp_child argp_children[] = {
    {&argp_solver_dbin, 0, "Solver options", 2},
    { 0 }
};

struct argp argp_search_dbin_random = {
    options,
    parse_opt,
    "SEARCH-TYPE",
    doc,
    argp_children
};

