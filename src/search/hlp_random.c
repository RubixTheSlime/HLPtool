#include "hlp_random.h"
#include <immintrin.h>
#include <time.h>
#include <stdlib.h>

static int verbosity;

uint64_t rand_uint64() {
    uint64_t result = 0;
    for (int i = 0; i < 8; i++) result = (result << 8) | (rand() & 0xff);
    return result;
}

uint64_t rand_hex_kperm(int n, int k) {
    uint64_t result = 0;
    uint16_t used = 0;
    for (int i=n; i > n-k; i--) {
        int value = rand() % i;
        value = _tzcnt_u16(_pdep_u32(1 << value, ~used));
        used |= 1 << value;
        result = result << 4 | value;
    }
    return result ;
}

uint64_t rand_hex_perm(int length) {
    return rand_hex_kperm(length, length);
}


char hex_digit(int value) {
    if (value < 10) return value + '0';
    return value - 10 + 'a';
}

void randomize_map(char* dest, int group) {
    dest[16] = 0;
    if (!group) {
        for (int i=0; i<16; i++) {
            dest[i] = hex_digit(rand() % 16);
        }
        return;
    }

    // technically should get a random combination, but a kperm is
    // easier and has the same effect
    uint64_t kperm = rand_hex_kperm(16, group);
    uint64_t map = kperm;

    // fill in the rest of values
    for (int i=0; i < 16-group; i++) {
        int index = rand() % group;
        int digit = (kperm >> index * 4) & 15;
        map = (map << 4) | digit;
    }

    uint64_t shuffle_perm = rand_hex_perm(16);
    // shuffle to make the beginning not a kperm
    for (int i=0; i < 16; i++) {
        int index = shuffle_perm & 15;
        shuffle_perm >>= 4;
        dest[i] = hex_digit((map >> index*4) & 15);
    }
}

static void random_search(int count, int group) {
    for (int i=0; i<count; i++) {
        char map[17];
        randomize_map(map, group);
        hlp_print_search(map);
    }
}

enum LONG_OPTION {
    LONG_OPTION_RANDOM_SEED = 1000,
    LONG_OPTION_RANDOM_SEARCH_GROUP
};

static const char doc[] =
"Solve random HLP cases"
;

static const struct argp_option options[] = {
    { "trials", 'n', "N", 0, "Solve n random cases, default 1" },
    { "seed", LONG_OPTION_RANDOM_SEED, "SEED", 0, "Set the random seed, uses system clock if not set" },
    { "unique-values", LONG_OPTION_RANDOM_SEARCH_GROUP, "N", 0, "Only check cases with N unique outputs, 0 for any (default)" },
    { 0 }
};

static error_t parse_opt(int key, char* arg, struct argp_state *state) {
    struct arg_settings_search_hlp_random* settings = state->input;
    switch (key) {
        case 'n':
            settings->trials = atoi(arg);
            break;
        case LONG_OPTION_RANDOM_SEARCH_GROUP:
            int group = atoi(arg);
            if (group < 1 || group > 16)
                argp_error(state, "%s unique outputs is impossible", arg);
            else
                settings->group = group;
            break;
        case LONG_OPTION_RANDOM_SEED:
            settings->seed = atoi(arg);
            break;
        case ARGP_KEY_INIT:
            settings->trials = 1;
            settings->group = 0;
            settings->seed = clock();
            settings->settings_solver_hex.global = settings->global;
            state->child_inputs[0] = &settings->settings_solver_hex;
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
    {&argp_solver_hex, 0, "Solver options", 2},
    { 0 }
};

struct argp argp_search_hlp_random = {
    options,
    parse_opt,
    "SEARCH-TYPE",
    doc,
    argp_children
};

