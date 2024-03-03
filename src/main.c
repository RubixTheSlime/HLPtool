#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>
#include <time.h>
#include <locale.h>
#include "bitonic_sort.h"

#include "arg_global.h"
#include "solver/hlp_solve.h"
#include "command/cat.h"
#include "command/hex.h"
#include "command/dbin_command.h"
#include "search/hlp_random.h"
#include "search/dbin_random.h"

union arg_settings_sub {
    struct arg_settings_solver_hex solver_hex;
    struct arg_settings_command_hex command_hex;
    struct arg_settings_command_cat command_cat;
    struct arg_settings_search_hlp_random search_hlp_random;
    struct arg_settings_search_dbin_random search_dbin_random;
};

struct subcommand_entry {
    char* name;
    struct argp* argp;
    size_t global_pointer_offset;
};


const char *argp_program_version = "version 1.1-dev";

int global_verbosity;

error_t process_subcommand(const char* name, struct argp_state* state, struct argp* argp_struct, void* input) {
    int argc = state->argc - state->next + 1;
    char** argv = &state->argv[state->next - 1];
    /* input->global = state->input; */

    char* argv0 =  argv[0];
    argv[0] = malloc(strlen(state->name) + strlen(name) + 2);
    if(!argv[0])
        argp_failure(state, 1, ENOMEM, 0);

    sprintf(argv[0], "%s %s", state->name, name);

    error_t error = argp_parse(argp_struct, argc, argv, 0, &argc, input);

    free(argv[0]);
    argv[0] = argv0;

    state->next += argc - 1;
    return error;
}

// every one of these offsets should be 0 anyways, but just to be sure
const struct subcommand_entry subcommand_entries[] = {
    { "hex", &argp_command_hex, offsetof(struct arg_settings_command_hex, global) },
    { "hlp", &argp_command_hex, offsetof(struct arg_settings_command_hex, global) },
    { "2bin", &argp_command_dbin, offsetof(struct arg_settings_command_dbin, global) },
    { "search-hlp-random", &argp_search_hlp_random, offsetof(struct arg_settings_search_hlp_random, global) },
    { "search-2bin-random", &argp_search_dbin_random, offsetof(struct arg_settings_search_dbin_random, global) },
};

static const char doc_global[] =
"collection of assorted tools relating to HLP"
"\v"
"Supported subcommands:\n"
"  hex, hlp     Find a solution for the vanilla hex layer problem\n"
"  2bin         Find a solution for the dual binary problem\n"
"  search-*     Automated searchers\n"
"  search       List available searchers\n"
"note that global options must be provided BEFORE the subcommand\n"
;

static const char doc_search[] =
"Searchers available:\n"
"  search-hlp-random\n"
"  search-2bin-random\n"
;

static const struct argp_option options_global[] = {
    { "verbose", 'v', "LEVEL", OPTION_ARG_OPTIONAL, "Increase or set verbosity" },
    { "quiet", 'q', 0, 0, "Suppress additional info" },
    { 0 }
};

static error_t parse_opt_global(int key, char* arg, struct argp_state *state) {
    struct arg_settings_global* settings = state->input;
    switch (key) {
        case 'v':
            if (arg)
                settings->verbosity = atoi(arg);
            else
                settings->verbosity++;
            break;
        case 'q':
            settings->verbosity = 0;
            break;
        case ARGP_KEY_INIT:
            settings->verbosity = 1;
            break;
        case ARGP_KEY_NO_ARGS:
            argp_state_help(state, stderr, ARGP_HELP_USAGE | ARGP_HELP_SHORT_USAGE | ARGP_HELP_SEE);
            return 1;
        case ARGP_KEY_ARG:
            union arg_settings_sub settings_sub;
            for (int i=0; i < sizeof(subcommand_entries) / sizeof(struct subcommand_entry); i++) {
                if (!strcmp(arg, subcommand_entries[i].name)) {
                    // this should always be safe (ie, not technically works, actually safe)
                    *(struct arg_settings_global**) ( (void*) &settings_sub + subcommand_entries[i].global_pointer_offset ) = settings;
                    return process_subcommand(arg, state, subcommand_entries[i].argp, &settings_sub);
                }
            }
            if (!strcmp(arg, "search")) {
                fprintf(stderr, doc_search);
                return 0;
            }
            argp_error(state, "unrecognized subcommand: %s", arg);
    }
    return 0;
}

static struct argp argp_global = {
    options_global,
    parse_opt_global,
    "SUBCOMMAND [ARGUMENTS]",
    doc_global
};

void print_arrays4x16x8(uint8_t* arrays) {
    for (int i = 0; i < 64; i++) {
        printf("%3u", arrays[i]);
        if (i % 16 == 15)
            printf("\n");
        else
            printf(", ");
    }
}

void test() {
    uint8_t arrays[64];
    for (int i = 0; i < 64; i++)
        arrays[i] = rand() & 0xff;
    print_arrays4x16x8(arrays);
    bitonic_sort4x16x8(arrays);
    printf("\n");
    print_arrays4x16x8(arrays);

}

int main(int argc, char** argv) {
    if (!__builtin_cpu_supports("avx2")) {
        printf("this program requires a CPU that supports AVX2, which yours doesn't. sorry, you're just plain out of luck.\n");
        return 1;
    }
    /* test(); return 0; */
    setlocale(LC_NUMERIC, "");

    struct arg_settings_global settings;
    error_t argp_error = argp_parse(&argp_global, argc, argv, ARGP_IN_ORDER, 0, &settings);
    if (argp_error) return argp_error;

    return 0;
}
