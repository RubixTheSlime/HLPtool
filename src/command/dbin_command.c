#include "dbin_command.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char** append_str(char** str1, char* str2) { 
    if (!str2) return str1;

    if (*str1) {
        char* new_str = malloc(strlen(*str1) + strlen(str2) + 1);
        strcpy(new_str, *str1);
        if (str2) strcat(new_str, str2);
        free(*str1);
        *str1 = new_str;
    } else {
        *str1 = malloc(strlen(str2) + 1);
        strcpy(*str1, str2);
    }

    return str1;
}
enum LONG_OPTIONS {
    LONG_OPTION_MAX_DEPTH = 1000,
    LONG_OPTION_ACCURACY,
    LONG_OPTION_CACHE_SIZE
};

static const char doc[] =
"Find a solution to the dual binary problem"
;

static const struct argp_option options[] = {
    { "transpose", 't', 0, 0, "transpose the input" },
    { "swap", 's', 0, 0, "swap bits within pairs" },
    { 0 }
};

static error_t parse_opt(int key, char* arg, struct argp_state *state) {
    struct arg_settings_command_dbin* settings = state->input;
    switch (key) {
        case ARGP_KEY_ARG:
            for (char* c = arg; *c; c++) {
                uint64_t mask = 0;
                switch (*c) {
                    case '0': mask = 1; break;
                    case '1': mask = (uint64_t) 1 << 32; break;
                    case 'x':
                    case 'X':
                    case '.': break;
                    case '\n':
                    case '\t':
                    case '\r':
                    case ' ': settings->bit_count--; break;
                    default: argp_error(state, "unexpected symbol: %c", *c);
                }
                int offset = settings->transpose ? ((settings->bit_count % 2) * 16 + (settings->bit_count / 2)) : settings->bit_count;
                if (settings->reverse) offset ^= 16;
                settings->bits |= mask << offset;

                settings->bit_count++;
                if (settings->bit_count > 32) argp_error(state, "too many bits");
            }
            break;
        case 't':
            settings->transpose = 1;
            break;
        case 'r':
            settings->reverse = 1;
            break;
        case ARGP_KEY_INIT:
            settings->bit_count = 0;
            settings->bits = 0;
            settings->transpose = 0;
            settings->reverse = 0;
            settings->settings_solver_dbin.global = settings->global;
            state->child_inputs[0] = &settings->settings_solver_dbin;
            break;
        case ARGP_KEY_SUCCESS:
            dbin_print_solve(settings->bits);
            break;
        case ARGP_KEY_NO_ARGS:
            argp_state_help(state, stderr, ARGP_HELP_USAGE | ARGP_HELP_SHORT_USAGE | ARGP_HELP_SEE);
            return 1;
    }
    return 0;
}

static struct argp_child argp_children[] = {
    {&argp_solver_dbin, 0, 0, 0},
    { 0 }
};

struct argp argp_command_dbin = {
    options,
    parse_opt,
    "FUNCTION",
    doc,
    argp_children
};


