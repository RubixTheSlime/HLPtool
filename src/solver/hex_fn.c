#include "hex_fn.h"

#include <stdbool.h>
#include <stdint.h>
#include <immintrin.h>
#include <stdio.h>

#include "../vector_tools.h"

typedef __m256i fn_set_t;

const __m128i identity_fn = {0x0706050403020100, 0x0f0e0d0c0b0a0908};

static hex_set_t *fn_set_as_array(fn_set_t *set) {
    return (hex_set_t *) set;
}

static const hex_set_t *fn_set_as_const_array(const fn_set_t *set) {
    return (const hex_set_t *) set;
}

static __m128i *hex_fn_as_xmm(hex_fn_t *hex_fn) {
    return (__m128i *) hex_fn;
}

int clamp_hex(int x) {
    return x < 0 ? 0 : x > 15 ? 15 : x;
}

hex_fn_t hex_fn_identity() {
    hex_fn_t res;
    _mm_store_si128(hex_fn_as_xmm(&res), identity_fn);
    return res;
}

hex_fn_t hex_fn_of_constant(int k) {
    return _mm_set1_epi8((uint8_t) k);
}

hex_fn_t hex_fn_from_byte_array(const uint8_t *data) {
    hex_fn_t res;
    _mm_storeu_si128(hex_fn_as_xmm(&res), _mm_loadu_si128((const __m128i *) data));
    return res;
}

hex_fn_t hex_fn_compose(hex_fn_t a, hex_fn_t b) {
    return _mm_shuffle_epi8(a, b);
}

int hex_fn_get(hex_fn_t hex_fn, int input) {
    return ((uint8_t *) &hex_fn)[input];
}

void hex_fn_set(hex_fn_t *hex_fn, int input, int output) {
    ((uint8_t *) hex_fn)[input] = output;
}

hex_fn_t hex_fn_with(hex_fn_t hex_fn, int input, int output) {
    hex_fn_t res = hex_fn;
    hex_fn_set(&res, input, output);
    return res;
}

bool hex_fn_is_identity(hex_fn_t hex_fn) {
    return hex_fn_is_equal(hex_fn, identity_fn);
}

int hex_fn_is_constant(hex_fn_t hex_fn) {
    return hex_fn_is_equal(hex_fn, _mm_broadcastb_epi8(hex_fn)) ? hex_fn_get(hex_fn, 0) : -1;
}

bool hex_fn_is_equal(hex_fn_t a, hex_fn_t b) {
    return _mm_movemask_epi8(_mm_cmpeq_epi64(a, b)) == 0xffff;
}

packed_hex_fn_t hex_fn_pack(hex_fn_t hex_fn) {
    return pack_xmm_to_uint(hex_fn);
}

hex_fn_t hex_fn_unpack(packed_hex_fn_t packed_hex_fn) {
    return unpack_uint_to_xmm(packed_hex_fn);
}

hex_fn_t hex_fn_load(hex_fn_t *unaligned_hex_fn) {
    return _mm_loadu_si128(unaligned_hex_fn);
}

void hex_fn_store(hex_fn_t *dst, hex_fn_t src) {
    _mm_storeu_si128(dst, src);
}

int hex_fn_sprint(char *dst, hex_fn_t hex_fn) {
    char *p = dst;
    for (int i = 0; i < 16; i++) {
        if (i % 4 == 0 && i > 0) {
            p += sprintf(p, "_");
        }
        // let's be extra safe
        int value = hex_fn_get(hex_fn, i);
        if (value >= 0 && value < 16) {
            p += sprintf(p, "%X", value);
        } else {
            p += sprintf(p, "?");
        }
    }
    return (int) (p - dst);
}

fn_set_t fn_set_new_empty() {
    return _mm256_setzero_si256();
}

fn_set_t fn_set_new_full() {
    return _mm256_set1_epi8(UINT8_MAX);
}

fn_set_t fn_set_new_identity() {
    fn_set_t res = fn_set_new_empty();
    for (int i = 0; i < 16; i++) {
        fn_set_set_bit(&res, i, i, true);
    }
    return res;
}

fn_set_t fn_set_new_containing(hex_fn_t hex_fn) {
    // todo optimize
    fn_set_t res;
    uint8_t *src = (uint8_t *) &hex_fn;
    for (int i = 0; i < 16; i++) {
        fn_set_set_io(&res, i, 1 << src[i]);
    }
    return res;
}

bool fn_set_get_bit(fn_set_t set, const int input, const int output) {
    return hex_set_get(((hex_set_t *) &set)[input], output);
}

void fn_set_set_bit(fn_set_t *set, const int input, const int output, const bool value) {
    hex_set_set((hex_set_t *) set + input, output, value);
}

fn_set_t fn_set_with_bit(fn_set_t set, int input, int output, bool value) {
    fn_set_t res = set;
    fn_set_set_bit(&res, input, output, value);
    return res;
}

hex_set_t fn_set_get_io(fn_set_t set, int input) {
    return fn_set_as_const_array(&set)[input];
}

void fn_set_set_io(fn_set_t *set, const int input, const hex_set_t slice) {
    fn_set_as_array(set)[input] = slice;
}

fn_set_t fn_set_with_io(fn_set_t set, int input, hex_set_t slice) {
    fn_set_t res = set;
    fn_set_set_io(&res, input, slice);
    return res;
}

hex_set_t fn_set_get_oi(const fn_set_t set, const int output) {
    __m256i adjusted = _mm256_slli_epi16(_mm256_and_si256(_mm256_srli_epi16(set, output), _mm256_set1_epi16(1)), 15);
    __m128i a = _mm256_castsi256_si128(adjusted);
    __m128i b = DOWNWARD_YMM(adjusted);
    __m128i merged = _mm_packs_epi16(a, b);
    return _mm_movemask_epi8(merged);
}

void fn_set_set_oi(fn_set_t *set, const int output, const hex_set_t slice) {
    for (int i = 0; i < 16; i++) {
        fn_set_set_bit(set, i, output, slice & (1 << i));
    }
}

fn_set_t fn_set_with_oi(fn_set_t set, int output, hex_set_t slice) {
    fn_set_t res = set;
    fn_set_set_oi(&res, output, slice);
    return res;
}

bool fn_set_is_super(fn_set_t super, fn_set_t sub) {
    return _mm256_movemask_epi8(_mm256_cmpeq_epi16(_mm256_andnot_si256(super, sub), _mm256_setzero_si256())) ==
           UINT32_MAX;
}

bool fn_set_is_equal(fn_set_t a, fn_set_t b) {
    return _mm256_movemask_epi8(_mm256_cmpeq_epi16(_mm256_cmpeq_epi64(a, b), _mm256_setzero_si256())) == 0;
}

static bool fn_set_contains_singleton(fn_set_t set, fn_set_t fn) {
    return _mm256_movemask_epi8(_mm256_cmpeq_epi16(_mm256_and_si256(set, fn), _mm256_setzero_si256())) == 0;
}

bool fn_set_contains_identity(const fn_set_t set) {
    return fn_set_contains_singleton(set, fn_set_new_identity());
}

bool fn_set_contains_fn(fn_set_t set, hex_fn_t hex_fn) {
    return fn_set_contains_singleton(set, fn_set_new_containing(hex_fn));
}

fn_set_t fn_set_pre_mul_fn(const fn_set_t set, const hex_fn_t hex_fn) {
    //todo optimize
    uint8_t *fn = (uint8_t *) &hex_fn;
    const hex_set_t *src = fn_set_as_const_array(&set);
    fn_set_t res = fn_set_new_empty();
    hex_set_t *dst = fn_set_as_array(&res);
    for (int i = 0; i < 16; i++) {
        dst[i] = src[fn[i]];
    }
    return res;
}

fn_set_t fn_set_pre_div_fn(const fn_set_t set, const hex_fn_t hex_fn) {
    uint8_t *fn = (uint8_t *) &hex_fn;
    const hex_set_t *src = fn_set_as_const_array(&set);
    fn_set_t res = _mm256_cmpeq_epi64(set, set);
    hex_set_t *dst = fn_set_as_array(&res);
    for (int i = 0; i < 16; i++) { dst[i] = -1; }
    for (int i = 0; i < 16; i++) {
        dst[fn[i]] &= src[i];
    }
    return res;
}

fn_set_t fn_set_post_mul_fn(const fn_set_t set, const hex_fn_t hex_fn) {
    __m128i fn = hex_fn;
    __m128i byte_mask = _mm_srli_si128(_mm_set1_epi8(-1), 15);
    __m256i src = set;
    __m256i res = fn_set_new_empty();
    __m256i bit_mask = _mm256_set1_epi16(1);
    for (int i = 0; i < 16; i++) {
        __m128i output = _mm_and_si128(fn, byte_mask);
        fn = _mm_srli_si128(fn, 1);
        __m256i vbit = _mm256_and_si256(src, bit_mask);
        res = _mm256_or_si256(res, _mm256_sll_epi16(vbit, output));
        src = _mm256_srli_epi16(src, 1);
    }
    return res;
}

fn_set_t fn_set_post_div_fn(const fn_set_t set, const hex_fn_t hex_fn) {
    __m128i fn = hex_fn;
    __m128i byte_mask = _mm_srli_si128(_mm_set1_epi8(-1), 15);
    __m256i res = fn_set_new_empty();
    for (int i = 0; i < 16; i++) {
        __m128i output = _mm_and_si128(fn, byte_mask);
        fn = _mm_srli_si128(fn, 1);
        __m256i vbit = _mm256_slli_epi16(_mm256_srl_epi16(set, output), 15);
        res = _mm256_or_si256(_mm256_srli_epi16(res, 1), vbit);
    }
    return res;
}

fn_set_t fn_set_inverse(fn_set_t set) {
    fn_set_t res;
    for (int i = 0; i < 16; i++) {
        fn_set_set_io(&res, i, fn_set_get_oi(set, i));
    }
    return res;
}

fn_set_t fn_set_load(fn_set_t *unaligned_fn_set) {
    return _mm256_loadu_si256(unaligned_fn_set);
}

void fn_set_store(fn_set_t *dst, fn_set_t src) {
    _mm256_storeu_si256(dst, src);
}

int fn_set_sprint(char *dst, fn_set_t fn_set) {
    char *p = dst;
    p += sprintf(p, "{");
    bool first = true;
    for (int i = 0; i < 16; i++) {
        hex_set_t hex_set = fn_set_get_io(fn_set, i);
        p += sprintf(p, "%s%X:", first ? "" : "  ", i);
        p += hex_set_sprint(p, hex_set);
        first = false;
    }
    p += sprintf(p, "}");

    return (int) (p - dst);
}

hex_set_t hex_set_empty() {
    return 0;
}

hex_set_t hex_set_full() {
    return -1;
}

bool hex_set_is_empty(hex_set_t hex_set) {
    return hex_set == 0;
}

bool hex_set_is_full(hex_set_t hex_set) {
    return hex_set == UINT16_MAX;
}

int hex_set_len(hex_set_t hex_set) {
    return _popcnt32(hex_set);
}

bool hex_set_get(const hex_set_t hex_set, int index) {
    return hex_set & (1 << index);
}

void hex_set_set(hex_set_t *hex_set, int index, bool value) {
    *hex_set = hex_set_with(*hex_set, index, value);
}

hex_set_t hex_set_with(hex_set_t hex_set, int index, bool value) {
    uint16_t mask = 1 << index;
    return (hex_set & ~mask) | (value ? mask : 0);
}

hex_set_t hex_set_union(hex_set_t a, hex_set_t b) {
    return a | b;
}

hex_set_t hex_set_intersect(hex_set_t a, hex_set_t b) {
    return a & b;
}

hex_set_t hex_set_diff(hex_set_t a, hex_set_t b) {
    return a & ~b;
}

hex_set_t hex_set_sym_diff(hex_set_t a, hex_set_t b) {
    return a ^ b;
}

hex_set_t hex_set_complement(hex_set_t set) {
    return ~set;
}

int hex_set_sprint(char *dst, hex_set_t hex_set) {
    char *p = dst;
    p += sprintf(p, "{");
    if (hex_set_is_full(hex_set)) {
        p += sprintf(p, "*");
    } else {
        uint16_t mask = hex_set;
        bool first = true;
        while (mask) {
            if (!first) p += sprintf(p, " ");
            first = false;
            int range_start = _tzcnt_u16(mask);
            mask = ~(mask | (mask - 1));
            int range_end = _tzcnt_u16(mask);
            mask = ~(mask | (mask - 1));
            int len = range_end - range_start;
            if (len == 1) {
                p += sprintf(p, "%X", range_start);
            } else if (len == 2) {
                p += sprintf(p, "%X %X", range_start, range_start + 1);
            } else {
                p += sprintf(p, "%X-%X", range_start, range_end - 1);
            }
        }
    }
    p += sprintf(p, "}");
    return (int) (p - dst);
}
