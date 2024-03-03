#include "redstone.h"
#include "aa_tree.h"
#include "vector_tools.h"
#include "stdio.h"
#include "time.h"
#include "stdlib.h"


static int verbosity = 0;

__m128i hex_layer128(__m128i start, uint16_t config) {
    // adjust mode if rotated
    // this makes it so the three bits in upper byte of config are independent
    config += (config & 0x400) >> 2;

    __m128i back1 = start;
    __m128i side2 = start;

    // unpack config
    __m128i v_config = _mm_cvtsi32_si128(config);
    __m128i back2 = _mm_broadcastb_epi8(v_config);
    __m128i side1 = _mm_and_si128(_mm_srli_epi64(back2, 4), LO_HALVES_4_128);
    back2 = _mm_and_si128(back2, LO_HALVES_4_128);

    // shift left then arith right so bit we shift to msb gets cast to whole register
    v_config = _mm_shuffle_epi32(v_config, 0);
    __m128i mode2 = _mm_srai_epi32(_mm_slli_epi32(v_config, 31 - 8 - 0), 31);
    __m128i mode1 = _mm_srai_epi32(_mm_slli_epi32(v_config, 31 - 8 - 1), 31);
    __m128i rotate = _mm_srai_epi32(_mm_slli_epi32(v_config, 31 - 8 - 2), 31);

    // use xor to conditionally swap back1 and side1
    rotate = _mm_and_si128(rotate, _mm_xor_si128(side1, back1));
    back1 = _mm_xor_si128(back1, rotate);
    side1 = _mm_xor_si128(side1, rotate);

    // apply the comparators
    __m128i output1 = _mm_andnot_si128(_mm_cmpgt_epi8(side1, back1), _mm_sub_epi8(back1, _mm_and_si128(side1, mode1)));
    __m128i output2 = _mm_andnot_si128(_mm_cmpgt_epi8(side2, back2), _mm_sub_epi8(back2, _mm_and_si128(side2, mode2)));
    __m128i output = _mm_max_epi8(output1, output2);

    // pack back into uint
    return output;
}

uint64_t hex_layer64(uint64_t start, uint16_t config) {
    return pack_xmm_to_uint(hex_layer128(unpack_uint_to_xmm(start), config));
}

struct precomputed_hex_layer* precomputed_hex_layer_history[32] = { 0 };

int round_up(int n, int factor) {
    return ((n - 1) / factor + 1) * factor;
}

#define LAYER_COUNT_ESTIMATE 1024
#define MAP_ARRAY_ALIGNMENT 4

//precompute of layers into lut, proceding layers deduplicated for lower branching
struct precomputed_hex_layer* precompute_hex_layers(int group, int direction) {
    int history_index = group - 1 + 16 * (direction < 0);
    if (precomputed_hex_layer_history[history_index]) {
        return precomputed_hex_layer_history[history_index];
    }

    int layer_count = 0;
    uint16_t* layer_configs_tmp = malloc(LAYER_COUNT_ESTIMATE * sizeof(uint16_t));

    aa* unique_next_layers_tree = aa_new(cmp_int64);
    uint64_t* tree_data = malloc(LAYER_COUNT_ESTIMATE * sizeof(uint64_t));
    uint64_t identity = IDENTITY_PERM_PK64;
    aa_add(unique_next_layers_tree, &identity, NULL);

    clock_t time_start = clock();
    if (verbosity >= 3) printf("starting layer precompute\n");

    // identify the unique first layers
    for(int conf = 0; conf < HEX_CONFIG_COUNT; conf++) {
        uint64_t output = hex_layer64(IDENTITY_PERM_PK64, conf);
        // skip if doesn't pass tests
        if (get_group64(output) < group) continue;
        if (aa_find(unique_next_layers_tree, &output)) continue;

        // passed, add it
        tree_data[layer_count] = output;
        aa_add(unique_next_layers_tree, tree_data + layer_count, NULL);
        layer_configs_tmp[layer_count] = conf;
        layer_count++;
    }
    free(tree_data);
    aa_free(unique_next_layers_tree);

    // now set up the array of layers
    // first is always identity, ie the first layer
    struct precomputed_hex_layer* layers = malloc((layer_count + 1) * sizeof(struct precomputed_hex_layer));
    layers[0].config = 0;
    layers[0].map = IDENTITY_PERM_PK64;

    for (int i = 0; i < layer_count; i++) {
        layers[i + 1].config = layer_configs_tmp[i];
        layers[i + 1].map = hex_layer64(IDENTITY_PERM_PK64, layer_configs_tmp[i]);
    }
    free(layer_configs_tmp); // no longer needed

    // need to re set up tree data to allocate new space, realloc wont work
    tree_data = malloc((layer_count + 1) * layer_count * sizeof(uint64_t));
    unique_next_layers_tree = aa_new(cmp_int64);
    aa_add(unique_next_layers_tree, &identity, NULL);

    int next_layer_count = 0;
    int map_spaces_needed = 0;
    if (verbosity >= 3) printf("starting next layer precompute\n");

    // identify the next layers
    int* next_layer_indices = malloc((layer_count + 1) * layer_count * sizeof(int));
    int* next_layer_starts = malloc((layer_count + 1) * sizeof(int));
    int* next_map_starts = malloc((layer_count + 1) * sizeof(int));
    for(int first_layer_i = 0; first_layer_i < layer_count + 1; first_layer_i++) {
        struct precomputed_hex_layer* first_layer = layers + first_layer_i;
        next_layer_starts[first_layer_i] = next_layer_count;
        next_map_starts[first_layer_i] = map_spaces_needed;
        first_layer->next_layer_count = 0;
        // don't both applying identity layer, start at 1
        for (int second_layer_i = 1; second_layer_i < layer_count + 1; second_layer_i++) {
            struct precomputed_hex_layer* second_layer = layers + second_layer_i;

            uint64_t output = direction < 0 ? 
                apply_mapping_packed64(second_layer->map, first_layer->map) :
                apply_mapping_packed64(first_layer->map, second_layer->map);
            if (get_group64(output) < group) continue;

            if (aa_find(unique_next_layers_tree, &output)) continue;
            tree_data[next_layer_count] = output;
            aa_add(unique_next_layers_tree, tree_data + next_layer_count, NULL);

            next_layer_indices[next_layer_count] = second_layer_i;
            next_layer_count++;
            first_layer->next_layer_count++;
        }
        // the maps require certain alignment to ensure they don't overlap, as
        // they are expected to be handled in bulk with vector processing
        map_spaces_needed += round_up(first_layer->next_layer_count, MAP_ARRAY_ALIGNMENT);
    }
    aa_free(unique_next_layers_tree);
    free(tree_data);

    // fill the next layer data
    // both of these will just be one big block containing multiple arrays
    struct precomputed_hex_layer** next_layer_array = malloc(next_layer_count * sizeof(struct precomputed_hex_layer*));
    uint64_t* next_map_array = calloc(map_spaces_needed, sizeof(uint64_t));

    for (int layer_index = 0; layer_index < layer_count + 1; layer_index++) {
        struct precomputed_hex_layer* layer = layers + layer_index;
        layer->next_layers = next_layer_array + next_layer_starts[layer_index];
        layer->next_layer_luts = next_map_array + next_map_starts[layer_index];

        // fill in the arrays
        for (int next_layer_index = 0; next_layer_index < layer->next_layer_count; next_layer_index++) {
            layer->next_layers[next_layer_index] = layers + next_layer_indices[next_layer_starts[layer_index] + next_layer_index];
            layer->next_layer_luts[next_layer_index] = layer->next_layers[next_layer_index]->map;
        }
    }
    free(next_layer_indices);
    free(next_layer_starts);
    free(next_map_starts);

    if (verbosity >= 3) {
        printf("layer precompute done in %.2fms\n", (double)(clock() - time_start) / CLOCKS_PER_SEC * 1000);
        printf("layers computed:%d, total next layers:%'ld\n", layer_count, next_layer_count - layer_count);
    }
    precomputed_hex_layer_history[history_index] = layers;
    return layers;
}

void free_precomputed_hex_layers() {
    for (int i = 0; i < 16; i++) {
        if (precomputed_hex_layer_history[i]) {
            free(precomputed_hex_layer_history[i]->next_layers);
            free(precomputed_hex_layer_history[i]->next_layer_luts);
            free(precomputed_hex_layer_history[i]);
        }
    }
}

uint32_t dbin_layer128(__m128i input, uint16_t config) {
    // unpack map and config
    __m128i back1 = input;
    __m128i side2 = input;

    // unlike normal layer(), using ymms is easier
    __m128i back2 = _mm_set1_epi8(config);
    __m128i side1 = _mm_and_si128(_mm_srli_epi64(back2, 4), LO_HALVES_4_128);
    back2 = _mm_and_si128(back2, LO_HALVES_4_128);

    __m256i backs = _mm256_permute2x128_si256(_mm256_castsi128_si256(back1), _mm256_castsi128_si256(back2), 0x20);
    __m256i sides = _mm256_permute2x128_si256(_mm256_castsi128_si256(side1), _mm256_castsi128_si256(side2), 0x20);

    // shift left then arith right so bit we shift to msb gets cast to whole register
    const __m256i mode_shifts = {22, 22, 23, 23}; // 31 - 8 - n
    __m256i modes = _mm256_srai_epi32(_mm256_sllv_epi64(_mm256_set1_epi32(config), mode_shifts), 31);

    // apply the comparators
    __m256i outputs = _mm256_andnot_si256(_mm256_cmpgt_epi8(sides, backs), _mm256_sub_epi8(backs, _mm256_and_si256(sides, modes)));

    // mingle together
    outputs = _mm256_max_epi8(outputs, _mm256_add_epi8(_mm256_permute4x64_epi64(outputs, PERMQ_REV_2x128_256), UINT256_MAX));

    // get which ones are currently at least 1
    return _mm256_movemask_epi8(_mm256_cmpgt_epi8(outputs, _mm256_setzero_si256()));
}

uint32_t dbin_layer64(uint64_t input, uint16_t config) {
    return dbin_layer128(little_endian_uint_to_xmm(input), config);
}

uint32_t dbin_layer_packed64(uint64_t input, uint16_t config) {
    return dbin_layer128(unpack_uint_to_xmm(input), config);
}

uint64_t apply_hex_chain(uint64_t start, uint16_t* chain, int length) {
    for (int i = 0; i < length; i++) {
        start = hex_layer64(start, chain[i]);
    }
    return start;
}

void print_chain(uint16_t* chain, int length) {
    const char layer_strings[][16] = {
        "%X, %X",
        "%X, *%X",
        "*%X, %X",
        "*%X, *%X",
        "^%X, *%X",
        "^*%X, %X"
    };
    for (int i = 0; i < length; i++) {
        uint16_t conf = chain[i];
        printf(layer_strings[conf >> 8], (conf >> 4) & 15, conf & 15);
        if (i < length - 1) printf(";  ");
    }
}



enum LONG_OPTIONS {
    LONG_OPTIONS_BLANK
};

static error_t parse_opt(int key, char* arg, struct argp_state *state) {
    struct arg_settings_redstone* settings = state->input;
    switch (key) {
        case ARGP_KEY_SUCCESS:
            verbosity = settings->global->verbosity;
            break;
    }
    return 0;
}

// unlike other parts, there will likely be many different versions of the argp
// options for this, as we only want to include the parts relevant for the
// given part of the program

static const struct argp_option options[] = {
    { 0 }
};

struct argp argp_redstone = {
    options,
    parse_opt
};
