#include "redstone.h"
#include "aa_tree.h"
#include "vector_tools.h"
#include "stdio.h"
#include "time.h"
#include <stdlib.h>
#include <glib.h>
#include <lauxlib.h>
#include <string.h>


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

struct precomputed_hex_layer *precomputed_hex_layer_history[32] = {0};

int round_up(int n, int factor) {
    return ((n - 1) / factor + 1) * factor;
}

#define LAYER_COUNT_ESTIMATE 1024
#define MAP_ARRAY_ALIGNMENT 4

//precompute of layers into lut, proceding layers deduplicated for lower branching
struct precomputed_hex_layer *precompute_hex_layers(int group, int direction) {
    int history_index = group - 1 + 16 * (direction < 0);
    if (precomputed_hex_layer_history[history_index]) {
        return precomputed_hex_layer_history[history_index];
    }

    int layer_count = 0;
    uint16_t *layer_configs_tmp = malloc(LAYER_COUNT_ESTIMATE * sizeof(uint16_t));

    GHashTable *unique_next_layers = g_hash_table_new(g_int64_hash, g_int64_equal);
    uint64_t *table_data = malloc(LAYER_COUNT_ESTIMATE * sizeof(uint64_t));
    uint64_t identity = IDENTITY_PERM_PK64;
    g_hash_table_add(unique_next_layers, &identity);

    clock_t time_start = clock();
    if (verbosity >= 3) printf("starting layer precompute\n");

    // identify the unique first layers
    for (int conf = 0; conf < HEX_CONFIG_COUNT; conf++) {
        uint64_t output = hex_layer64(IDENTITY_PERM_PK64, conf);
        // skip if doesn't pass tests
        if (get_group64(output) < group) continue;
        if (g_hash_table_contains(unique_next_layers, &output)) continue;

        // passed, add it
        table_data[layer_count] = output;
        g_hash_table_add(unique_next_layers, table_data + layer_count);
        layer_configs_tmp[layer_count] = conf;
        layer_count++;
    }
    free(table_data);

    // now set up the array of layers
    // first is always identity, ie the first layer
    struct precomputed_hex_layer *layers = malloc((layer_count + 1) * sizeof(struct precomputed_hex_layer));
    layers[0].config = 0;
    layers[0].map = IDENTITY_PERM_PK64;

    for (int i = 0; i < layer_count; i++) {
        layers[i + 1].config = layer_configs_tmp[i];
        layers[i + 1].map = hex_layer64(IDENTITY_PERM_PK64, layer_configs_tmp[i]);
    }
    free(layer_configs_tmp); // no longer needed

    // need to re set up tree data to allocate new space, realloc wont work
    table_data = malloc((layer_count + 1) * layer_count * sizeof(uint64_t));
    g_hash_table_remove_all(unique_next_layers);
    g_hash_table_add(unique_next_layers, &identity);

    int next_layer_count = 0;
    int map_spaces_needed = 0;
    if (verbosity >= 3) printf("starting next layer precompute\n");

    // identify the next layers
    int *next_layer_indices = malloc((layer_count + 1) * layer_count * sizeof(int));
    int *next_layer_starts = malloc((layer_count + 1) * sizeof(int));
    int *next_map_starts = malloc((layer_count + 1) * sizeof(int));
    for (int first_layer_i = 0; first_layer_i < layer_count + 1; first_layer_i++) {
        struct precomputed_hex_layer *first_layer = layers + first_layer_i;
        next_layer_starts[first_layer_i] = next_layer_count;
        next_map_starts[first_layer_i] = map_spaces_needed;
        first_layer->next_layer_count = 0;
        // don't both applying identity layer, start at 1
        for (int second_layer_i = 1; second_layer_i < layer_count + 1; second_layer_i++) {
            struct precomputed_hex_layer *second_layer = layers + second_layer_i;

            uint64_t output = direction < 0
                                  ? apply_mapping_packed64(second_layer->map, first_layer->map)
                                  : apply_mapping_packed64(first_layer->map, second_layer->map);
            if (get_group64(output) < group) continue;

            if (g_hash_table_contains(unique_next_layers, &output)) continue;
            table_data[next_layer_count] = output;
            g_hash_table_add(unique_next_layers, table_data + next_layer_count);

            next_layer_indices[next_layer_count] = second_layer_i;
            next_layer_count++;
            first_layer->next_layer_count++;
        }
        // the maps require certain alignment to ensure they don't overlap, as
        // they are expected to be handled in bulk with vector processing
        map_spaces_needed += round_up(first_layer->next_layer_count, MAP_ARRAY_ALIGNMENT);
    }
    g_hash_table_destroy(unique_next_layers);
    free(table_data);

    // fill the next layer data
    // both of these will just be one big block containing multiple arrays
    struct precomputed_hex_layer **next_layer_array = malloc(next_layer_count * sizeof(struct precomputed_hex_layer *));
    uint64_t *next_map_array = calloc(map_spaces_needed, sizeof(uint64_t));

    for (int layer_index = 0; layer_index < layer_count + 1; layer_index++) {
        struct precomputed_hex_layer *layer = layers + layer_index;
        layer->next_layers = next_layer_array + next_layer_starts[layer_index];
        layer->next_layer_luts = next_map_array + next_map_starts[layer_index];

        // fill in the arrays
        for (int next_layer_index = 0; next_layer_index < layer->next_layer_count; next_layer_index++) {
            layer->next_layers[next_layer_index] =
                    layers + next_layer_indices[next_layer_starts[layer_index] + next_layer_index];
            layer->next_layer_luts[next_layer_index] = layer->next_layers[next_layer_index]->map;
        }
    }
    free(next_layer_indices);
    free(next_layer_starts);
    free(next_map_starts);

    if (verbosity >= 3) {
        printf("layer precompute done in %.2fms\n", (double) (clock() - time_start) / CLOCKS_PER_SEC * 1000);
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
    __m256i outputs = _mm256_andnot_si256(_mm256_cmpgt_epi8(sides, backs),
                                          _mm256_sub_epi8(backs, _mm256_and_si256(sides, modes)));

    // mingle together
    outputs = _mm256_max_epi8(
        outputs, _mm256_add_epi8(_mm256_permute4x64_epi64(outputs, PERMQ_REV_2x128_256), UINT256_MAX));

    // get which ones are currently at least 1
    return _mm256_movemask_epi8(_mm256_cmpgt_epi8(outputs, _mm256_setzero_si256()));
}

uint32_t dbin_layer64(uint64_t input, uint16_t config) {
    return dbin_layer128(little_endian_uint_to_xmm(input), config);
}

uint32_t dbin_layer_packed64(uint64_t input, uint16_t config) {
    return dbin_layer128(unpack_uint_to_xmm(input), config);
}

uint64_t apply_hex_chain(uint64_t start, uint16_t *chain, int length) {
    for (int i = 0; i < length; i++) {
        start = hex_layer64(start, chain[i]);
    }
    return start;
}

const char star_names[][32] = {
    "%s%s%X, %X",
    "%s%s%X, *%X",
    "%s%s*%X, %X",
    "%s%s*%X, *%X",
    "%s%s^%X, *%X",
    "%s%s*%X, ^%X"
};

const char v_names[][32] = {
    "%s%s%X^, %X>",
    "%s%s%X^, %X>*",
    "%s%s%X^*, %X>",
    "%s%s%X^*, %X>*",
    "%s%s%X>, %X>*",
    "%s%s%X^*, %X^"
};

char* chain_to_str(const uint16_t *chain, int length, enum LAYER_NOTATION notation) {
    const char (*layer_strings)[32];
    switch (notation) {
        case LAYER_NOTATION_V: layer_strings = v_names; break;
        case LAYER_NOTATION_STAR: layer_strings = star_names; break;
    }

    char *res = malloc(sizeof(char));
    res[0] = '\0';
    for (int i = 0; i < length; i++) {
        uint16_t conf = chain[i];
        char *res_old = res;
        asprintf(&res, layer_strings[conf >> 8], res, i > 0 ? ";  " : "", (conf >> 4) & 15, conf & 15);
        free(res_old);
    }

    return res;
}

void print_chain(const uint16_t *chain, int length, enum LAYER_NOTATION notation) {
    char *res = chain_to_str(chain, length, notation);
    printf(res);
    free(res);
}

enum LAYER_NOTATION get_layer_notation_by_name(const char *name) {
    if (name == NULL) return LAYER_NOTATION_V;
    if (strcmp(name, "star") == 0) return LAYER_NOTATION_STAR;
    return LAYER_NOTATION_V;
}

static void lua2c_get_half(lua_State *L, int index, int *barrel, int *side, int *sub) {
    lua_rawgeti(L, -1, index);
    lua_getfield(L, -1, "barrel");
    *barrel = lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, -1, "side");
    // invert because too lazy to actually fix
    *side = !lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, -1, "sub");
    *sub = lua_toboolean(L, -1);
    lua_pop(L, 2);
}

const uint8_t modes_by_key[] = {0, 5, 5, 2, 0, 1, 2, 3, 0, 2, 1, 3, 4, 4, 4, 4};
// 0 = 0, 1 = index 1 barrel, 2 = index 2 barrel, 3 = min, 4 = max
const uint8_t as_by_key[] = {3, 1, 2, 3, 1, 1, 1, 1, 2, 2, 2, 2, 0, 1, 2, 0};
const uint8_t bs_by_key[] = {0, 2, 1, 0, 2, 2, 2, 2, 1, 1, 1, 1, 4, 2, 1, 4};

int lua2c_chain(lua_State *L, int idx, uint16_t **chain) {
    *chain = malloc(sizeof(uint16_t) * 16);
    for (int i = 0;; i++) {
        lua_rawgeti(L, idx, i + 1);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            return i;
        }
        int barrel1, side1, sub1;
        int barrel2, side2, sub2;
        lua2c_get_half(L, 1, &barrel1, &side1, &sub1);
        lua2c_get_half(L, 2, &barrel2, &side2, &sub2);
        lua_pop(L, 1);
        int mode_key = side1 << 3 | side2 << 2 | sub1 << 1 | sub2;
        uint16_t mode = modes_by_key[mode_key];
        int a_key = as_by_key[mode_key];
        int b_key = bs_by_key[mode_key];
        int xor_key = barrel1 > barrel2 ? 3 : 0;
        if (a_key > 2) a_key = (a_key - 2) ^ xor_key;
        if (b_key > 2) b_key = (b_key - 2) ^ xor_key;
        uint16_t a = a_key == 0 ? 0 : a_key == 1 ? barrel1 : barrel2;
        uint16_t b = b_key == 0 ? 0 : b_key == 1 ? barrel1 : barrel2;
        uint16_t value = mode << 8 | a << 4 | b;
        if (i > 8 && i & (i - 1) == 0) {
            *chain = realloc(*chain, sizeof(uint16_t) * i * 2);
        }
        (*chain)[i] = value;
    }
}

static void c2lua_add_half(lua_State *L, int index, int barrel, bool side, bool sub) {
    lua_newtable(L);
    lua_pushinteger(L, barrel);
    lua_setfield(L, -2, "barrel");
    lua_pushboolean(L, side);
    lua_setfield(L, -2, "side");
    lua_pushboolean(L, sub);
    lua_setfield(L, -2, "sub");
    lua_rawseti(L, -2, index);
}

void c2lua_chain(lua_State *L, const uint16_t *chain, int length) {
    lua_newtable(L);
    for (int index = 1; index <= length; index++) {
        uint16_t config = chain[index - 1];
        uint32_t mode_mask = 1 << (config >> 8);
        lua_newtable(L);
        c2lua_add_half(L, 1, (config >> 4) & 15, (mode_mask & 0b101111) != 0, (mode_mask & 0b101100) != 0);
        c2lua_add_half(L, 2, config & 15, (mode_mask & 0b100000) != 0, (mode_mask & 0b011010) != 0);
        lua_rawseti(L, -2, index);
    }
}

struct arst {
    __m256i things;
};



enum LONG_OPTIONS {
    LONG_OPTIONS_BLANK
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arg_settings_redstone *settings = state->input;
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
    {0}
};

struct argp argp_redstone = {
    options,
    parse_opt
};
