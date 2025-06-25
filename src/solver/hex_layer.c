//
// Created by rubix on 6/22/25.
//

#include "hex_layer.h"

#include <glib.h>
#include <stdio.h>
#define LAYER_DATA_BUFFER_SIZE 1024

static bool initted = false;
static hex_layer_data_t HEX_LAYER_DATA[LAYER_DATA_BUFFER_SIZE];
static uint16_t HEX_LAYER_DATA_SIZE = 0;
static hex_layer_data_id_t HEX_LAYER_TO_FN_ID[HEX_LAYER_COUNT];
static hex_layer_t HEX_LAYER_CONFIG_LISTS[HEX_LAYER_COUNT];

static int comparator(int back, int side, bool sub, bool swap) {
    if (swap) return comparator(side, back, sub, false);
    if (back < side) return 0;
    if (sub) return back - side;
    return back;
}

static hex_fn_t comparator_fn(uint16_t mask) {
    int value = mask & 15;
    bool sub = (mask >> 4) & 1;
    bool neg = (mask >> 5) & 1;
    hex_fn_t fn;
    for (int i = 0; i < 16; i++) {
        hex_fn_set(&fn, i, comparator(i, value, sub, neg));
    }
    return fn;
}

void init_hex_layer_data() {
    if (initted) return;
    initted = true;
    GHashTable *hash_table = g_hash_table_new_full(g_int64_hash, g_int64_equal, NULL, NULL);
    uint64_t *keys = malloc(sizeof(uint64_t) * LAYER_DATA_BUFFER_SIZE);
    for (hex_layer_t hex_layer = 0; hex_layer < HEX_LAYER_COUNT; ++hex_layer) {
        hex_fn_t fn = hex_layer_output(hex_layer);
        uint64_t key = hex_fn_pack(fn);
        hex_layer_data_t *layer_data = g_hash_table_lookup(hash_table, &key);
        if (layer_data == NULL) {
            keys[HEX_LAYER_DATA_SIZE] = key;
            layer_data = HEX_LAYER_DATA + HEX_LAYER_DATA_SIZE;
            layer_data->fn = fn;
            layer_data->configs = malloc(sizeof(hex_layer_t) * 8);
            layer_data->config_count = 0;
            g_hash_table_insert(hash_table, keys + HEX_LAYER_DATA_SIZE, layer_data);
            HEX_LAYER_DATA_SIZE++;
        }
        if ((layer_data->config_count & (layer_data->config_count - 1)) == 0 && layer_data->config_count >= 8) {
            layer_data->configs = realloc(layer_data->configs, layer_data->config_count * sizeof(hex_layer_t) * 2);
        }
        layer_data->configs[layer_data->config_count] = hex_layer;
        layer_data->config_count++;
    }
    g_hash_table_destroy(hash_table);
    free(keys);

    // layer data table can be reordered here

    hex_layer_t *config_slice = HEX_LAYER_CONFIG_LISTS;
    for (hex_layer_data_id_t layer_data_id = 0; layer_data_id < HEX_LAYER_DATA_SIZE; ++layer_data_id) {
        hex_layer_data_t *layer_data = HEX_LAYER_DATA + layer_data_id;
        for (int i = 0; i < layer_data->config_count; i++) {
            config_slice[i] = layer_data->configs[i];
            HEX_LAYER_TO_FN_ID[layer_data->configs[i]] = layer_data_id;
        }
        free(layer_data->configs);
        layer_data->configs = config_slice;
        config_slice += layer_data->config_count;
    }
}

hex_fn_t hex_layer_output(hex_layer_t config) {
    hex_fn_t fn;
    hex_fn_t fn_a = comparator_fn(config & 0x3f);
    hex_fn_t fn_b = comparator_fn((config >> 6) & 0x3f);
    for (int i = 0; i < 16; i++) {
        int a = hex_fn_get(fn_a, i);
        int b = hex_fn_get(fn_b, i);
        hex_fn_set(&fn, i, a > b ? a : b);
    }
    return fn;
}

hex_layer_data_id_t hex_layer_get_data_id(hex_layer_t layer) {
    return HEX_LAYER_TO_FN_ID[layer];
}

const hex_layer_data_t *hex_layer_get_data(hex_layer_t layer) {
    return hex_layer_get_data_by_id(hex_layer_get_data_id(layer));
}

const hex_layer_data_t *hex_layer_get_data_by_id(hex_layer_data_id_t layer_data_id) {
    return HEX_LAYER_DATA + layer_data_id;
}

hex_layer_data_id_t hex_layer_data_get_data_id(const hex_layer_data_t *layer_data) {
    return layer_data - HEX_LAYER_DATA;
}

hex_layer_data_id_t hex_layer_unique_count() {
    return HEX_LAYER_DATA_SIZE;
}

bool hex_layer_is_classic(hex_layer_t layer, int variant) {
    int mode_mask = layer >> 4 & 3 | layer >> 8 & 12;
    switch (variant) {
        case 6: if (mode_mask == 0b0001) return true;
        case 5: if (mode_mask == 0b1110) return true;
        case 4: if ((mode_mask & 0b1010) == 0b1000) return true;
        default: return false;
    }
}

static int get_part(int x, int half, int start, int length) {
    start += half * 6;
    return (x >> start) & ((1 << length) - 1);
}

static int with_part(int x, int y, int half, int start, int length) {
    start += half * 6;
    return x & ~((1 << start + length) - (1 << start)) | ((y & ((1 << length) - 1)) << start);
}

int hex_layer_get_barrel(hex_layer_t layer, int half) {
    return get_part(layer, half, 0, 4);
}

void hex_layer_set_barrel(hex_layer_t *layer, int half, int value) {
    *layer = hex_layer_with_barrel(*layer, half, value);
}

hex_layer_t hex_layer_with_barrel(hex_layer_t layer, int half, int barrel) {
    return with_part(layer, barrel, half, 0, 4);
}

bool hex_layer_get_sub(hex_layer_t layer, int half) {
    return get_part(layer, half, 4, 1);
}

void hex_layer_set_sub(hex_layer_t *layer, int half, bool value) {
    *layer = hex_layer_with_sub(*layer, half, value);
}

hex_layer_t hex_layer_with_sub(hex_layer_t layer, int half, bool sub) {
    return with_part(layer, sub ? 1 : 0, half, 4, 1);
}

bool hex_layer_get_neg(hex_layer_t layer, int half) {
    return get_part(layer, half, 5, 1);
}

void hex_layer_set_neg(hex_layer_t *layer, int half, bool value) {
    *layer = hex_layer_with_neg(*layer, half, value);
}

hex_layer_t hex_layer_with_neg(hex_layer_t layer, int half, bool neg) {
    return with_part(layer, neg ? 1 : 0, half, 5, 1);
}

int hex_layer_sprint(char *dst, hex_layer_t layer) {
    char *p = dst;
    for (int half = 0; half < 2; half++) {
        p += sprintf(p, "%s%X%c%s", half == 0 ? "" : ", ", hex_layer_get_barrel(layer, half),
                     hex_layer_get_neg(layer, half) ? '>' : '^', hex_layer_get_sub(layer, half) ? "*" : "");
    }

    return (int) (p - dst);
}
