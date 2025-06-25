#ifndef HEX_FN_H
#define HEX_FN_H

#include <immintrin.h>
#include <lua.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>

typedef __m128i hex_fn_t;
typedef uint64_t packed_hex_fn_t;
typedef __m256i fn_set_t;
typedef uint16_t hex_set_t;

extern int clamp_hex(int x);

extern hex_fn_t hex_fn_identity();

extern hex_fn_t hex_fn_of_constant(int k);

extern hex_fn_t hex_fn_from_byte_array(const uint8_t* data);

extern hex_fn_t hex_fn_compose(hex_fn_t a, hex_fn_t b);

extern int hex_fn_get(hex_fn_t hex_fn, int input);

extern void hex_fn_set(hex_fn_t *hex_fn, int input, int output);

extern hex_fn_t hex_fn_with(hex_fn_t hex_fn, int input, int output);

extern bool hex_fn_is_identity(hex_fn_t hex_fn);

extern int hex_fn_is_constant(hex_fn_t hex_fn);

extern bool hex_fn_is_equal(hex_fn_t a, hex_fn_t b);

extern hex_set_t hex_fn_out_set(hex_fn_t hex_fn);

extern packed_hex_fn_t hex_fn_pack(hex_fn_t hex_fn);

extern hex_fn_t hex_fn_unpack(packed_hex_fn_t packed_hex_fn);

extern hex_fn_t hex_fn_load(hex_fn_t *unaligned_hex_fn);

extern void hex_fn_store(hex_fn_t *dst, hex_fn_t src);

extern int hex_fn_sprint(char *dst, hex_fn_t hex_fn);



extern fn_set_t fn_set_new_empty();

extern fn_set_t fn_set_new_full();

extern fn_set_t fn_set_new_identity();

extern fn_set_t fn_set_new_containing(hex_fn_t hex_fn);


extern bool fn_set_get_bit(fn_set_t set, int input, int output);

extern void fn_set_set_bit(fn_set_t *set, int input, int output, bool value);

extern fn_set_t fn_set_with_bit(fn_set_t set, int input, int output, bool value);


extern hex_set_t fn_set_get_io(fn_set_t set, int input);

extern void fn_set_set_io(fn_set_t *set, int input, hex_set_t slice);

extern fn_set_t fn_set_with_io(fn_set_t set, int input, hex_set_t slice);


extern hex_set_t fn_set_get_oi(fn_set_t set, int output);

extern void fn_set_set_oi(fn_set_t *set, int output, hex_set_t slice);

extern fn_set_t fn_set_with_oi(fn_set_t set, int output, hex_set_t slice);


extern bool fn_set_is_super(fn_set_t super, fn_set_t sub);

extern bool fn_set_is_equal(fn_set_t super, fn_set_t sub);

extern bool fn_set_sat(fn_set_t fn_set);

extern bool fn_set_contains_identity(fn_set_t set);

extern bool fn_set_contains_fn(fn_set_t set, hex_fn_t hex_fn);

extern fn_set_t fn_set_pre_mul_fn(fn_set_t set, hex_fn_t hex_fn);

extern fn_set_t fn_set_pre_div_fn(fn_set_t set, hex_fn_t hex_fn);

extern fn_set_t fn_set_post_mul_fn(fn_set_t set, hex_fn_t hex_fn);

extern fn_set_t fn_set_post_div_fn(fn_set_t set, hex_fn_t hex_fn);

extern fn_set_t fn_set_inverse(fn_set_t set);

extern hex_set_t fn_set_out_set(fn_set_t fn_set);

extern fn_set_t fn_set_load(fn_set_t *unaligned_fn_set);

extern void fn_set_store(fn_set_t *dst, fn_set_t src);

extern int fn_set_sprint(char *dst, fn_set_t fn_set);



extern hex_set_t hex_set_empty();

extern hex_set_t hex_set_full();

extern bool hex_set_is_empty(hex_set_t hex_set);

extern bool hex_set_is_full(hex_set_t hex_set);

extern int hex_set_len(hex_set_t hex_set);

extern bool hex_set_get(hex_set_t hex_set, int index);

extern void hex_set_set(hex_set_t *hex_set, int index, bool value);

extern hex_set_t hex_set_with(hex_set_t hex_set, int index, bool value);

extern hex_set_t hex_set_union(hex_set_t a, hex_set_t b);

extern hex_set_t hex_set_intersect(hex_set_t a, hex_set_t b);

extern hex_set_t hex_set_diff(hex_set_t a, hex_set_t b);

extern hex_set_t hex_set_sym_diff(hex_set_t a, hex_set_t b);

extern hex_set_t hex_set_complement(hex_set_t set);

extern int hex_set_sprint(char *dst, hex_set_t hex_set);

#endif //HEX_FN_H
