#ifndef HEX_LAYER_H
#define HEX_LAYER_H
#include <stdint.h>

#include "hex_fn.h"

#define HEX_LAYER_COUNT 4096

typedef uint16_t hex_layer_data_id_t;
typedef uint16_t hex_layer_t;

typedef struct {
    hex_fn_t fn;
    hex_layer_t *configs;
    int config_count;
} hex_layer_data_t;

extern void init_hex_layer_data();

extern hex_fn_t hex_layer_output(hex_layer_t config);

extern hex_layer_data_id_t hex_layer_get_data_id(hex_layer_t layer);

extern const hex_layer_data_t *hex_layer_get_data(hex_layer_t layer);

extern const hex_layer_data_t *hex_layer_get_data_by_id(hex_layer_data_id_t layer_data_id);

extern hex_layer_data_id_t hex_layer_data_get_data_id(const hex_layer_data_t *layer_data);

extern hex_layer_data_id_t hex_layer_unique_count();

extern bool hex_layer_is_classic(hex_layer_t layer, int variant);

extern int hex_layer_get_barrel(hex_layer_t layer, int half);

extern void hex_layer_set_barrel(hex_layer_t *layer, int half, int value);

extern hex_layer_t hex_layer_with_barrel(hex_layer_t layer, int half, int barrel);

extern bool hex_layer_get_sub(hex_layer_t layer, int half);

extern void hex_layer_set_sub(hex_layer_t *layer, int half, bool value);

extern hex_layer_t hex_layer_with_sub(hex_layer_t layer, int half, bool sub);

extern bool hex_layer_get_neg(hex_layer_t layer, int half);

extern void hex_layer_set_neg(hex_layer_t *layer, int half, bool value);

extern hex_layer_t hex_layer_with_neg(hex_layer_t layer, int half, bool neg);

extern int hex_layer_sprint(char *dst, hex_layer_t layer);

#endif //HEX_LAYER_H
