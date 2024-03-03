#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <immintrin.h>
#include "../aa_tree.h"
#include "hlp_solve.h"
#include <stdbool.h>
#include "../bitonic_sort.h"
#include "../vector_tools.h"
#include "../redstone.h"
#include "../cache.h"

struct hlp_solve_globals {
    struct __config__ {
        __m256i goal_min, goal_max, dont_care_mask, dont_care_post_sort_perm;
        int solve_type, dont_care_count, current_bfs_depth, group, accuracy;
    } config;

    struct __output__ {
        uint16_t* chain;
        int chain_length;
        int solutions_found;
    } output;

    struct __stats__ {
        long total_iterations;
        clock_t start_time;
    } stats;
};

static int verbosity = 1;

int global_max_depth;
int global_accuracy;

int is_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int to_hex(char c) {
    return c - (c <= '9' ? '0' : c <= 'F' ? 'A' - 10 : 'a' - 10);
}

int map_pair_contains_ranges(uint64_t mins, uint64_t maxs) {
    while (mins && maxs) {
        int min_val = mins & 15;
        int max_val = maxs & 15;
        if (min_val != min_val && !(min_val == 0 && max_val == 15)) return 1;
        mins >>= 4;
        maxs >>= 4;
    }
    return 0;
}

struct hlp_request parse_hlp_request_str(char* str) {
    struct hlp_request result = {0};
    if (!str){
        result.error = HLP_ERROR_NULL;
        return result;
    };
    if (!*str){
        result.error = HLP_ERROR_BLANK;
        return result;
    };
    int length = 0;
    char* c = str;
    while (*c) {
        if (*(c + 1) == '-') {
            if (!is_hex(*(c + 2))) {
                result.error = HLP_ERROR_MALFORMED;
                return result;
            }
            result.mins = (result.mins << 4) | to_hex(*c);
            result.maxs = (result.maxs << 4) | to_hex(*(c + 2));
            length++;
            c += 3;
            continue;
        }
        if (*c == '.' || *c == 'x' || *c == 'X') {
            result.mins = (result.mins << 4) | 0;
            result.maxs = (result.maxs << 4) | 15;
            length++;
            c++;
            continue;
        }
        if (*c == '[' || *c == ']') continue;
        if (is_hex(*c)) {
            result.mins = (result.mins << 4) | to_hex(*c);
            result.maxs = (result.maxs << 4) | to_hex(*c);
            length++;
            c++;
            continue;
        }
        result.error = HLP_ERROR_MALFORMED;
        return result;
    }
    int remaining_length = 16 - length;
 
    if (remaining_length < 0) {
        result.error = HLP_ERROR_TOO_LONG;
        return result;
    }
    result.mins <<= remaining_length * 4;
    result.maxs <<= remaining_length * 4;
    result.maxs |= ((uint64_t) 1 << (remaining_length * 4)) - 1;

    if (result.mins == result.maxs)
        result.solve_type = HLP_SOLVE_TYPE_EXACT;
    else if (map_pair_contains_ranges(result.mins, result.maxs))
        result.solve_type = HLP_SOLVE_TYPE_RANGED;
    else
        result.solve_type = HLP_SOLVE_TYPE_PARTIAL;

    return result;
}

#define COMBINE_RANGES_INNER(shift, s1, s2)\
    mask = _mm256_cmpeq_epi8(equality_reference, _mm256_s##s1##li_si256(equality_reference, shift));\
    mins_and_maxs.ymm0 = _mm256_max_epu8(mins_and_maxs.ymm0, _mm256_s##s2##li_si256(_mm256_and_si256(mins_and_maxs.ymm0, mask), shift));\
    mins_and_maxs.ymm1 = _mm256_max_epu8(mins_and_maxs.ymm1, _mm256_s##s2##li_si256(_mm256_and_si256(mins_and_maxs.ymm1, mask), shift));\

static ymm_pair_t combine_ranges(__m256i equality_reference, ymm_pair_t mins_and_maxs) {
    // we invert the max values so that after shifting things, any zeros
    // shifted in will not affect anything, as we only combine things with max
    // function
    mins_and_maxs.ymm1 = _mm256_xor_si256(mins_and_maxs.ymm1, UINT256_MAX);
    __m256i mask;

    COMBINE_RANGES_INNER(1, r, l);
    COMBINE_RANGES_INNER(2, r, l);
    COMBINE_RANGES_INNER(4, r, l);
    COMBINE_RANGES_INNER(8, r, l);

    COMBINE_RANGES_INNER(1, l, r);
    COMBINE_RANGES_INNER(2, l, r);
    COMBINE_RANGES_INNER(4, l, r);
    COMBINE_RANGES_INNER(8, l, r);

    mins_and_maxs.ymm1 = _mm256_xor_si256(mins_and_maxs.ymm1, UINT256_MAX);
}

static int get_legal_dist_check_mask_ranged(struct hlp_solve_globals* globals, __m256i sorted_ymm, int threshhold) {
    __m256i final_indices = _mm256_and_si256(sorted_ymm, LO_HALVES_4_256);
    __m256i current = _mm256_and_si256(_mm256_srli_epi64(sorted_ymm, 4), LO_HALVES_4_256);
    ymm_pair_t final = {_mm256_shuffle_epi8(globals->config.goal_min, final_indices), _mm256_shuffle_epi8(globals->config.goal_max, final_indices)};
    final = combine_ranges(current, final);

    // if the min is higher than the max, that's all we need to know
    __m256i illegals = _mm256_cmpgt_epi8(final.ymm0, final.ymm1);
    int mask = -1;
    mask &= _mm256_testz_si256(LO_HALVES_128_256, illegals) | (_mm256_testc_si256(LO_HALVES_128_256, illegals) << 2);

    __m256i final_delta = _mm256_max_epi8(
            _mm256_sub_epi8(final.ymm0, _mm256_srli_si256(final.ymm1, 1)),
            _mm256_sub_epi8(_mm256_srli_si256(final.ymm0, 1), final.ymm1)
            );
    __m256i current_delta = _mm256_abs_epi8(_mm256_sub_epi8(_mm256_srli_si256(current, 1), current));

    uint32_t separations_mask = _mm256_movemask_epi8(_mm256_cmpgt_epi8(final_delta, current_delta)) & 0x7fff7fff;
    mask &= (_popcnt32(separations_mask & 0xffff) <= threshhold) | ((_popcnt32(separations_mask >> 16) <= threshhold) << 2);
    return mask;
}

static int get_legal_dist_check_mask_partial(struct hlp_solve_globals* globals, __m256i sorted_ymm, int threshhold) {
    __m256i final = _mm256_and_si256(sorted_ymm, LO_HALVES_4_256);
    __m256i current = _mm256_and_si256(_mm256_srli_epi64(sorted_ymm, 4), LO_HALVES_4_256);

    __m256i final_delta = _mm256_abs_epi8(_mm256_sub_epi8(_mm256_srli_si256(final, 1), final));
    __m256i current_delta = _mm256_abs_epi8(_mm256_sub_epi8(_mm256_srli_si256(current, 1), current));

    __m256i illegals = _mm256_and_si256(_mm256_cmpeq_epi8(current_delta, _mm256_setzero_si256()), final_delta);
    illegals = _mm256_and_si256(illegals, LOW_15_BYTES_256);
    int mask = -1;
    mask &= _mm256_testz_si256(LO_HALVES_128_256, illegals) | (_mm256_testc_si256(LO_HALVES_128_256, illegals) << 2);

    uint32_t separations_mask = _mm256_movemask_epi8(_mm256_cmpgt_epi8(final_delta, current_delta)) & 0x7fff7fff;
    mask &= (_popcnt32(separations_mask & 0xffff) <= threshhold) | ((_popcnt32(separations_mask >> 16) <= threshhold) << 2);
    return mask;
}

static int batch_apply_and_check_exact(
        struct hlp_solve_globals* globals,
        struct precomputed_hex_layer* layer,
        uint16_t* outputs,
        uint64_t input,
        int threshhold) {
    __m256i doubled_input = _mm256_permute4x64_epi64(_mm256_castsi128_si256(unpack_uint_to_xmm(input)), 0x44);

    // this contains extra bits to overwrite the current value on dont care entries
    __m256i doubled_goal;
    if (globals->config.solve_type == HLP_SOLVE_TYPE_RANGED) 
        doubled_goal = SHUFB_IDENTITY_256;
    else
        doubled_goal = _mm256_or_si256(globals->config.goal_min, globals->config.dont_care_mask);

    uint16_t* current_output = outputs;

    for (int i = (layer->next_layer_count - 1) / 4; i >= 0; i--) {
        ymm_pair_t quad = quad_unpack_map256(_mm256_loadu_si256(((__m256i*) layer->next_layer_luts) + i));
        quad.ymm0 = _mm256_shuffle_epi8(quad.ymm0, doubled_input);
        quad.ymm1 = _mm256_shuffle_epi8(quad.ymm1, doubled_input);

        ymm_pair_t sorted_quad = { _mm256_or_si256(doubled_goal, _mm256_slli_epi64(quad.ymm0, 4)),
            _mm256_or_si256(doubled_goal, _mm256_slli_epi64(quad.ymm1, 4)) };
        sorted_quad = bitonic_sort4x16x8_inner(sorted_quad);

        sorted_quad.ymm0 = _mm256_shuffle_epi8(sorted_quad.ymm0, globals->config.dont_care_post_sort_perm);
        sorted_quad.ymm1 = _mm256_shuffle_epi8(sorted_quad.ymm1, globals->config.dont_care_post_sort_perm);
        int mask;
        if (globals->config.solve_type == HLP_SOLVE_TYPE_RANGED)
            mask = get_legal_dist_check_mask_ranged(globals, sorted_quad.ymm0, threshhold) | (get_legal_dist_check_mask_ranged(globals, sorted_quad.ymm1, threshhold) << 1);
        else
            mask = get_legal_dist_check_mask_partial(globals, sorted_quad.ymm0, threshhold) | (get_legal_dist_check_mask_partial(globals, sorted_quad.ymm1, threshhold) << 1);
        if (i & (mask == 0)) continue;
        __m256i packed = quad_pack_map256(quad);

        for (int j = 3; j >= 0; j--) {
            *current_output = i * 4 + j;
            current_output += (mask >> j) & 1;
        }
    }

    return current_output - outputs;
}

static int get_min_group(uint64_t mins, uint64_t maxs) {
    // not great but works for now
    uint16_t bit_feild = 0;
    for(int i = 16; i; i--) {
        if ((mins & 15) == (maxs & 15))
            bit_feild |= 1 << (mins & 15);
        mins >>= 4;
        maxs >>= 4;
    }
    int result = _popcnt32(bit_feild);
    if (!result) return 1;
    return result;
}

//faster implementation of searching over the last layer while checking if you found the goal, unexpectedly big optimization
static int fast_last_layer_search(struct hlp_solve_globals* globals, uint64_t input, struct precomputed_hex_layer* layer) {
    __m256i doubled_input = _mm256_permute4x64_epi64(_mm256_castsi128_si256(unpack_uint_to_xmm(input)), 0x44);

    __m256i* quad_maps = (__m256i*) (layer->next_layer_luts);

    globals->stats.total_iterations += layer->next_layer_count;
    for (int i = (layer->next_layer_count - 1) / 4; i >= 0; i--) {
        ymm_pair_t quad = quad_unpack_map256(_mm256_loadu_si256(quad_maps + i));

        // determine if there are any spots that do not match up
        // if all zeros, that means they match
        // instead of split tests based on variant, the ranged test is only 2
        // cycles longer than the exact mode, so we just use ranged for
        // everything to avoid branches
        quad.ymm0 = _mm256_shuffle_epi8(quad.ymm0, doubled_input);
        quad.ymm1 = _mm256_shuffle_epi8(quad.ymm1, doubled_input);
        quad.ymm0 = _mm256_or_si256(_mm256_cmpgt_epi8(globals->config.goal_min, quad.ymm0), _mm256_cmpgt_epi8(quad.ymm0, globals->config.goal_max));
        quad.ymm1 = _mm256_or_si256(_mm256_cmpgt_epi8(globals->config.goal_min, quad.ymm1), _mm256_cmpgt_epi8(quad.ymm1, globals->config.goal_max));
        // no need to apply globals->config.dont_care_mask, they already will always succeed anyways
        if (i && _mm256_testnzc_si256(LO_HALVES_128_256, quad.ymm0) && _mm256_testnzc_si256(LO_HALVES_128_256, quad.ymm1)) continue;

        bool successes[] = {
            _mm256_testz_si256(LO_HALVES_128_256, quad.ymm0),
            _mm256_testz_si256(LO_HALVES_128_256, quad.ymm1),
            _mm256_testc_si256(LO_HALVES_128_256, quad.ymm0),
            _mm256_testc_si256(LO_HALVES_128_256, quad.ymm1)};

        for (int j=0; j<4; j++) {
            if (!successes[j]) continue;
            int index = i * 4 + j;
            globals->stats.total_iterations -= index;
            uint16_t config = layer->next_layers[index]->config;
            if (globals->output.solutions_found != -1) {
                globals->output.solutions_found++;
                continue;
            }
            globals->output.chain_length = globals->config.current_bfs_depth;
            if (globals->output.chain != 0) globals->output.chain[globals->config.current_bfs_depth - 1] = config;
            return 1;
        }
    }
    return 0;
}

// the most number of separations that can be found in the distance check before it prunes
static int get_dist_threshold(struct hlp_solve_globals* globals, int remaining_layers) {
    if (globals->config.accuracy == ACCURACY_REDUCED) return remaining_layers - (remaining_layers > 2);
    // n is always sufficient anyways for 15-16 outputs
    if (globals->config.accuracy == ACCURACY_NORMAL || globals->config.group > 14) return remaining_layers;
    // n+1 is always sufficient for 14 outputs
    if (globals->config.accuracy == ACCURACY_INCREASED || globals->config.group > 13) return remaining_layers + 1;

    // currently the best known general threshhold
    // +/-1 is for round up division
    return ((remaining_layers * 3 - 1) >> 1) + 1;
}

/* test to see if this map falls under a solution
 */
static int test_map(struct hlp_solve_globals* globals, uint64_t map) {
    __m128i xmm = unpack_uint_to_xmm(map);
    return _mm_testz_si128(_mm_or_si128(
                _mm_cmpgt_epi8(_mm256_castsi256_si128(globals->config.goal_min), xmm),
                _mm_cmpgt_epi8(xmm, _mm256_castsi256_si128(globals->config.goal_max))
                ), UINT128_MAX);
}


//main dfs recursive search function
static int dfs(struct hlp_solve_globals* globals, uint64_t input, int depth, struct precomputed_hex_layer* layer, uint16_t* staged_branches) {
    // test to see if we found a solution, even if we're not at the end. this
    // can happen even though it seems like it shouldn't
    if (test_map(globals, input)) {
        globals->output.chain_length = depth;
        return 1;
    }

    if(depth == globals->config.current_bfs_depth - 1) return fast_last_layer_search(globals, input, layer);
    globals->stats.total_iterations += layer->next_layer_count;
    int total_next_layers_identified = batch_apply_and_check_exact(
            globals,
            layer,
            staged_branches,
            input,
            get_dist_threshold(globals, globals->config.current_bfs_depth - depth - 1));

    for(int i = total_next_layers_identified - 1; i >= 0; i--) {
        struct precomputed_hex_layer* next_layer = layer->next_layers[staged_branches[i]];
        uint64_t output = apply_mapping_packed64(input, next_layer->map);

        //cache check
        if(cache_check(&main_cache, output, depth)) continue;

        //call next layers
        if(dfs(globals, output, depth + 1, next_layer, staged_branches + layer->next_layer_count)) {
            if (globals->output.chain != 0) globals->output.chain[depth] = next_layer->config;
            return 1;
        }
        if (verbosity < 3) continue;
        if(depth == 0 && globals->config.current_bfs_depth > 8) printf("done:%d/%d\n", total_next_layers_identified - i, total_next_layers_identified);
    }
    return 0;
}

static int init(struct hlp_solve_globals* globals, struct hlp_request request) {
    cache_init(&main_cache);
    globals->stats.start_time = clock();
    globals->config.solve_type = request.solve_type;
    globals->stats.total_iterations = 0;

    switch (globals->config.solve_type) {
        case HLP_SOLVE_TYPE_EXACT:
            globals->config.group = get_group64(request.mins);
            break;
        case HLP_SOLVE_TYPE_PARTIAL:
            globals->config.group = get_min_group(request.mins, request.maxs);
            break;
        default:
            printf("you found a search mode that isn't implemented\n");
            return 1;
    }
    
    globals->config.goal_min = _mm256_permute4x64_epi64(_mm256_castsi128_si256(big_endian_uint_to_xmm(request.mins)), 0x44);
    globals->config.goal_max = _mm256_permute4x64_epi64(_mm256_castsi128_si256(big_endian_uint_to_xmm(request.maxs)), 0x44);

    globals->config.dont_care_mask = _mm256_cmpeq_epi8(_mm256_sub_epi8(globals->config.goal_max, globals->config.goal_min), LO_HALVES_4_256);
    globals->config.dont_care_count = _popcnt32(_mm_movemask_epi8(_mm256_castsi256_si128(globals->config.dont_care_mask)));
    globals->config.dont_care_post_sort_perm = _mm256_min_epi8(SHUFB_IDENTITY_256, _mm256_set1_epi8(15 - globals->config.dont_care_count));

    return 0;
}

//main search loop
int single_search_inner(struct hlp_solve_globals* globals, struct precomputed_hex_layer* base_layer, int max_depth) {
    globals->config.current_bfs_depth = 1;

    while (globals->config.current_bfs_depth <= max_depth) {
        uint16_t* staged_branches = malloc(base_layer->next_layer_count * globals->config.current_bfs_depth * sizeof(uint16_t));
        int success = dfs(globals, IDENTITY_PERM_PK64, 0, base_layer, staged_branches);
        free(staged_branches);
        if (success) {
            if (verbosity >= 3) {
                printf("solution found at %.2fms\n", (double)(clock() - globals->stats.start_time) / CLOCKS_PER_SEC * 1000);
                printf("total iter over all: %'ld\n", globals->stats.total_iterations);
                cache_print_stats(&main_cache);
            }
            return globals->output.chain_length;
        }
        invalidate_cache(&main_cache);
        globals->config.current_bfs_depth++;

        if (verbosity < 2) continue;
        printf("search over layer %d done\n",globals->config.current_bfs_depth - 1);

        if (verbosity < 3) continue;
        printf("layer search done after %.2fms; %'ld iterations\n", (double)(clock() - globals->stats.start_time) / CLOCKS_PER_SEC * 1000, globals->stats.total_iterations);
    }
    if (verbosity >= 2) {
        printf("failed to beat depth\n");
        cache_print_stats(&main_cache);
    }
    return max_depth + 1;
}

int solve(struct hlp_request request, uint16_t* output_chain, int max_depth, enum search_accuracy accuracy) {
    struct hlp_solve_globals globals = {0};
    int requested_max_depth = max_depth;
    if (max_depth < 0 || max_depth > 31) max_depth = 31;

    if (init(&globals, request)) {
        printf("an error occurred\n");
        return requested_max_depth + 1;
    }

    struct precomputed_hex_layer* identity_layer = precompute_hex_layers(globals.config.group, 1);
    /* return requested_max_depth + 1; */

    if (request.mins == 0) {
        if (output_chain) output_chain[0] = 0x2f0;
        return 1;
    }

    globals.output.chain = output_chain;
    globals.output.solutions_found = -1;
    int solution_length = max_depth;

    if (verbosity >= 2) {
        if (accuracy > ACCURACY_REDUCED) printf("starting presearch\n");
        else printf("starting search\n");
    }

    // reduced accuracy search is sometimes faster than the others but
    // still often gets an optimal solution, so we start with that so the
    // "real" search can cut short if it doesn't find a better solution.
    // when it's not faster, the solution is found pretty fast anyways.
    globals.config.accuracy = ACCURACY_REDUCED;
    solution_length = single_search_inner(&globals, identity_layer, solution_length);

    if (solution_length == max_depth) solution_length = max_depth;
    if (accuracy == ACCURACY_REDUCED) return solution_length;
    long total_iter = globals.stats.total_iterations;
    globals.stats.total_iterations = 0;

    if (verbosity >= 2) printf("starting main search\n");

    globals.config.accuracy = accuracy;
    int result = single_search_inner(&globals, identity_layer, solution_length - 1);
    if (verbosity >= 2) printf("total iter across searches: %'ld\n", total_iter + globals.stats.total_iterations);
    if (result > max_depth) return requested_max_depth + 1;
    return result;
}

void print_hlp_map(uint64_t map) {
    struct hlp_request request = {map, map};
    print_hlp_request(request);
}

void print_hlp_request(struct hlp_request request) {
    for (int i = 15; i >= 0; i--) {
        if (i % 4 == 3 && i != 15) printf(" ");
        int min_val = (request.mins >> i * 4) & 15;
        int max_val = (request.maxs >> i * 4) & 15;
        if (min_val == max_val) {
            printf("%X", min_val);
            continue;
        }
        if (min_val == 0 && max_val == 15) {
            printf("X");
            continue;
        }
        printf("[%X-%X]", min_val, max_val);
    }
}

void hlp_print_search(char* map) {
    uint16_t result[32];
    struct hlp_request request = parse_hlp_request_str(map);
    switch (request.error) {
        case HLP_ERROR_NULL:
        case HLP_ERROR_BLANK:
            printf("Error: must provide a function to solve for\n");
            return;
        case HLP_ERROR_TOO_LONG:
            printf("Error: too many values are provided\n");
            return;
        case HLP_ERROR_MALFORMED:
            printf("Error: malformed expression\n");
            return;
    }

    if (verbosity > 0) {
        printf("searching for ");
        print_hlp_request(request);
        printf("\n");
    }

    int length = solve(request, result, global_max_depth, global_accuracy);

    if (length > global_max_depth) {
        if (verbosity > 0)
            printf("no result found\n");
    } else {
        if (verbosity > 0) {
            printf("result found, length %d", length);
            if (verbosity > 2 || request.solve_type != HLP_SOLVE_TYPE_EXACT) {
                printf(" (");
                print_hlp_map(apply_hex_chain(IDENTITY_PERM_BE64, result, length));
                printf(")");
            }
            printf(":  ");
        }
        print_chain(result, length);
        printf("\n");
    }
}

enum LONG_OPTIONS {
    LONG_OPTION_MAX_DEPTH = 1000,
    LONG_OPTION_ACCURACY,
    LONG_OPTION_CACHE_SIZE
};

static const struct argp_option options[] = {
    { "fast", 'f', 0, 0, "Equivilant to --accuracy -1" },
    { "perfect", 'p', 0, 0, "Equivilant to --accuracy 2" },
    { "max-layers", LONG_OPTION_MAX_DEPTH, "N", 0, "Limit results to chains up to N layers long" },
    { "accuracy", LONG_OPTION_ACCURACY, "LEVEL", 0, "Set search accuracy from -1 to 2, 0 being normal, 2 being perfect" },
    { "cache", LONG_OPTION_CACHE_SIZE, "N", 0, "Set the cache size to 2**N bytes. default: 26 (64MB)" },
    { 0 }
};

static error_t parse_opt(int key, char* arg, struct argp_state *state) {
    struct arg_settings_solver_hex* settings = state->input;
    switch (key) {
        case LONG_OPTION_ACCURACY:
            int level = atoi(arg);
            if (level < -1 || level > 2)
                argp_error(state, "%s is not a valid accuracy", arg);
            else
                global_accuracy = level;
            break;
        case 'f':
            return parse_opt(LONG_OPTION_ACCURACY, "-1", state);
        case 'p':
            return parse_opt(LONG_OPTION_ACCURACY, "2", state);
        case LONG_OPTION_MAX_DEPTH:
            global_max_depth = atoi(arg);
            break;
        case LONG_OPTION_CACHE_SIZE:
            main_cache.size_log = (atoi(arg) - 4);
            break;
        case ARGP_KEY_INIT:
            global_accuracy = ACCURACY_NORMAL;
            global_max_depth = 31;
            main_cache.size_log = 22;
            settings->settings_redstone.global = settings->global;
            state->child_inputs[0] = &settings->settings_redstone;
            break;
        case ARGP_KEY_SUCCESS:
            verbosity = settings->global->verbosity;
            break;
    }
    return 0;
}

static struct argp_child argp_children[] = {
    {&argp_redstone},
    { 0 }
};

struct argp argp_solver_hex = {
    options,
    parse_opt,
    0,
    0,
    argp_children
};


//#pragma GCC pop_options
