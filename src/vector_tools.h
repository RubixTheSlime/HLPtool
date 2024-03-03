#ifndef VECTOR_TOOLS_H
#define VECTOR_TOOLS_H
#include <stdint.h>
#include <immintrin.h>

#define CAT_CONST(a, b, w) (a & ((1 << w) - 1)) | ((b & ((1 << w) - 1)) << w)

#define CAT_CONST_8B(x0,x1,x2,x3,x4,x5,x6,x7)\
    (uint64_t) (x0 & 0xff) |\
    ((uint64_t) (x1 &0xff) << 8) |\
    ((uint64_t) (x2 &0xff) << 16) |\
    ((uint64_t) (x3 &0xff) << 24) |\
    ((uint64_t) (x4 &0xff) << 32) |\
    ((uint64_t) (x5 &0xff) << 40) |\
    ((uint64_t) (x6 &0xff) << 48) |\
    ((uint64_t) (x7 &0xff) << 56)

#define DOUBLE_XMM_TO_YMM_INNER(a,b) a, b, a, b
#define XMM_SHUFB_INNER(x0,x1,x2,x3,x4,x5,x6,x7,x8,x9,x10,x11,x12,x13,x14,x15) CAT_CONST_8B(x0,x1,x2,x3,x4,x5,x6,x7), CAT_CONST_8B(x8,x9,x10,x11,x12,x13,x14,x15)
#define XMM_SHUFB(...) ((const __m128i) {XMM_SHUFB_INNER(__VA_ARGS__)})
#define XMM2_SHUFB(...) ((const __m256i) {XMM_SHUFB_INNER(__VA_ARGS__), XMM_SHUFB_INNER(__VA_ARGS__)})
#define DOUBLE_XMM(x) _mm256_permute4x64_epi64(_mm256_castsi128_si256(x), 0x44)

typedef struct ymm_pair_s {
    __m256i ymm0;
    __m256i ymm1;
} ymm_pair_t;

#define UINT128_MAX ((__m128i) {-1, -1})
#define UINT256_MAX ((__m256i) {-1, -1, -1, -1})

#define DUPE_UINT_128(x)        ((const __m128i) {x, x})
#define DUPE_UINT_256(x)        ((const __m256i) {x, x, x, x})

#define BROADCAST_2x(w, x)      (((const uint64_t) x << w) | x)
#define BROADCAST_4x(w, x)      ((const uint64_t) BROADCAST_2x(w * 2, x) * BROADCAST_2x(w, 1))
#define BROADCAST_8x(w, x)      ((const uint64_t) BROADCAST_4x(w * 2, x) * BROADCAST_2x(w, 1))
#define BROADCAST_16x(w, x)     ((const uint64_t) BROADCAST_8x(w * 2, x) * BROADCAST_2x(w, 1))
#define BROADCAST_32x(w, x)     ((const uint64_t) BROADCAST_16x(w * 2, x) * BROADCAST_2x(w, 1))

#define LO_HALVES_1_64          ((const uint64_t) BROADCAST_32x(2, 1))
#define LO_HALVES_2_64          ((const uint64_t) BROADCAST_16x(4, 3))
#define LO_HALVES_4_64          ((const uint64_t) BROADCAST_8x(8, 15))
#define LO_HALVES_8_64          ((const uint64_t) BROADCAST_4x(16, UINT8_MAX))
#define LO_HALVES_16_64         ((const uint64_t) BROADCAST_2x(32, UINT16_MAX))
#define LO_HALVES_32_64         ((const uint64_t) UINT32_MAX)

#define HI_HALVES_1_64          ((const uint64_t) ~LO_HALVES_1_64)
#define HI_HALVES_2_64          ((const uint64_t) ~LO_HALVES_2_64)
#define HI_HALVES_4_64          ((const uint64_t) ~LO_HALVES_4_64)
#define HI_HALVES_8_64          ((const uint64_t) ~LO_HALVES_8_64)
#define HI_HALVES_16_64         ((const uint64_t) ~LO_HALVES_16_64)
#define HI_HALVES_32_64         ((const uint64_t) ~LO_HALVES_32_64)

// gcc likes to pessimize these to the point that it's generally faster to just load from memory
#define DECLARE_HALVES(side, iw, xw) extern const __m##xw##i side##_HALVES_## iw ##_##xw;
#define DECLARE_MANY_HALVES(side, xw) \
    DECLARE_HALVES(side, 1, xw) \
    DECLARE_HALVES(side, 2, xw) \
    DECLARE_HALVES(side, 4, xw) \
    DECLARE_HALVES(side, 8, xw) \
    DECLARE_HALVES(side, 16, xw) \
    DECLARE_HALVES(side, 32, xw)
DECLARE_MANY_HALVES(LO, 128);
DECLARE_MANY_HALVES(HI, 128);
DECLARE_MANY_HALVES(LO, 256);
DECLARE_MANY_HALVES(HI, 256);
#undef DECLARE_HALVES
#undef DECLARE_MANY_HALVES

// todo: get the others to work like these
#define LO_HALVES_64_128        _mm_srli_si128(UINT128_MAX, 8)
#define HI_HALVES_64_128        _mm_slli_si128(UINT128_MAX, 8)
#define LO_HALVES_64_256        _mm256_srli_si256(UINT256_MAX, 8)
#define HI_HALVES_64_256        _mm256_slli_si256(UINT256_MAX, 8)

#define LO_HALVES_128_256       _mm256_castsi128_si256(UINT128_MAX)
#define HI_HALVES_128_256       _mm256_xor_si256(LO_HALVES_128_256, UINT256_MAX)

#define LOW_15_BYTES_128 _mm_srli_si128(UINT128_MAX, 1)
#define LOW_15_BYTES_256 _mm256_srli_si256(UINT256_MAX, 1)

// packed, little endian, big endian
#define IDENTITY_PERM_PK64 ((uint64_t) 0x7f6e5d4c3b2a1908)
#define IDENTITY_PERM_LE64 ((uint64_t) 0xfedcba9876543210)
#define IDENTITY_PERM_BE64 ((uint64_t) 0x0123456789abcdef)

#define SHUFB_IDENTITY_128      XMM_SHUFB(  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15)
#define SHUFB_REV_2x8_128       XMM_SHUFB(  1,  0,  3,  2,  5,  4,  7,  6,  9,  8, 11, 10, 13, 12, 15, 14)
#define SHUFB_REV_4x8_128       XMM_SHUFB(  3,  2,  1,  0,  7,  6,  5,  4, 11, 10,  9,  8, 15, 14, 13, 12)
#define SHUFB_REV_8x8_128       XMM_SHUFB(  7,  6,  5,  4,  3,  2,  1,  0, 15, 14, 13, 12, 11, 10,  9,  8)
#define SHUFB_REV_16x8_128      XMM_SHUFB( 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0)
#define SHUFB_REV_2x16_128      XMM_SHUFB(  2,  3,  0,  1,  6,  7,  4,  5, 10, 11,  8,  9, 14, 15, 12, 13)
#define SHUFB_REV_4x16_128      XMM_SHUFB(  6,  7,  4,  5,  2,  3,  0,  1, 14, 15, 12, 13, 10, 11,  8,  9)
#define SHUFB_REV_8x16_128      XMM_SHUFB( 14, 15, 12, 13, 10, 11,  8,  9,  6,  7,  4,  5,  2,  3,  0,  1)

#define SHUFB_IDENTITY_256      DOUBLE_XMM(SHUFB_IDENTITY_128)
#define SHUFB_REV_2x8_256       DOUBLE_XMM(SHUFB_REV_2x8_128)
#define SHUFB_REV_4x8_256       DOUBLE_XMM(SHUFB_REV_4x8_128)
#define SHUFB_REV_8x8_256       DOUBLE_XMM(SHUFB_REV_8x8_128)
#define SHUFB_REV_16x8_256      DOUBLE_XMM(SHUFB_REV_16x8_128)
#define SHUFB_REV_2x16_256      DOUBLE_XMM(SHUFB_REV_2x16_128)
#define SHUFB_REV_4x16_256      DOUBLE_XMM(SHUFB_REV_4x16_128)
#define SHUFB_REV_8x16_256      DOUBLE_XMM(SHUFB_REV_8x16_128)

#define SHUFD_REV_2x32_256      0b10110001
#define SHUFD_REV_4x32_256      0b00011011
#define SHUFD_REV_2x64_256      0b01001110

#define PERMQ_REV_4x64_256      SHUFD_REV_4x32_256
#define PERMQ_REV_2x128_256     SHUFD_REV_2x64_256

#define DOWNWARD_YMM(x)         _mm256_castsi256_si128(_mm256_permute4x64_epi64(x, 0xee))

static __m128i unpack_uint_to_xmm(uint64_t uint) {
    __m128i input = _mm_cvtsi64_si128(uint);
    return _mm_and_si128(_mm_or_si128(_mm_slli_si128(input, 8), _mm_srli_epi64(input, 4)), LO_HALVES_4_128);
}

static uint64_t pack_xmm_to_uint(__m128i xmm) {
    return _mm_cvtsi128_si64(_mm_or_si128(_mm_slli_epi32(xmm, 4), _mm_srli_si128(xmm, 8)));
}

static __m128i little_endian_uint_to_xmm(uint64_t uint) {
    __m128i input = _mm_cvtsi64_si128(uint);
    return _mm_unpacklo_epi8(
            _mm_and_si128(input, LO_HALVES_4_128),
            _mm_and_si128(_mm_srli_epi64(input, 4), LO_HALVES_4_128)
            );
}

static uint64_t little_endian_xmm_to_uint(__m128i xmm) {
    __m128i merged = _mm_or_si128(_mm_and_si128(xmm, LO_HALVES_8_128), _mm_srli_epi16(xmm, 4));
    return _mm_cvtsi128_si64(_mm_packus_epi16(merged, _mm_setzero_si128()));
}

static __m128i big_endian_uint_to_xmm(uint64_t uint) {
    return _mm_shuffle_epi8(little_endian_uint_to_xmm(uint), SHUFB_REV_16x8_128);
}

static uint64_t big_endian_xmm_to_uint(__m128i xmm) {
    return little_endian_xmm_to_uint(_mm_shuffle_epi8(xmm, SHUFB_REV_16x8_128));
}

static ymm_pair_t quad_unpack_map256(__m256i packed) {
    const __m256i upack_shifts = {4, 0, 4, 0};
    ymm_pair_t pair = {
        _mm256_and_si256(_mm256_srlv_epi64(_mm256_shuffle_epi32(packed, 0x44), upack_shifts), LO_HALVES_4_256),
        _mm256_and_si256(_mm256_srlv_epi64(_mm256_shuffle_epi32(packed, 0xee), upack_shifts), LO_HALVES_4_256)};
    return pair;
}

static __m256i quad_pack_map256(ymm_pair_t unpacked) {
    return _mm256_blend_epi32(
            _mm256_or_si256(_mm256_srli_si256(unpacked.ymm0, 8), _mm256_slli_epi64(unpacked.ymm0, 4)),
            _mm256_or_si256(unpacked.ymm1, _mm256_slli_epi64(_mm256_slli_si256(unpacked.ymm1, 8), 4)),
            0b11001100
            );
}

static uint64_t apply_mapping_packed64(uint64_t first, uint64_t second) {
    return pack_xmm_to_uint(_mm_shuffle_epi8(unpack_uint_to_xmm(second), unpack_uint_to_xmm(first)));
}

//counts uniqe values, usefull for generalizable prune of layers that reduce too much from the get go
static int get_group64(uint64_t x) {
    uint16_t bit_feild = 0;
    for(int i = 0; i < 16; i++) {
        int ss = (x >> (i * 4)) & 15;
        bit_feild |= 1 << ss;
    }
    return _popcnt32(bit_feild);
}

static uint64_t reverse_movmask_64(uint8_t mask) {
    return _pdep_u64(mask, BROADCAST_8x(8, 1)) * UINT8_MAX;
}

static __m128i reverse_movmask_128(uint16_t mask) {
    return (__m128i) {
        reverse_movmask_64(mask),
        reverse_movmask_64(mask >> 8)
    };
}

static __m256i reverse_movmask_256(uint32_t mask) {
    return (__m256i) {
        reverse_movmask_64(mask),
        reverse_movmask_64(mask >> 8),
        reverse_movmask_64(mask >> 16),
        reverse_movmask_64(mask >> 24)
    };
}

/* for use with aa tree, sort, bsearch, etc
 * needed because simple casting 64 to 32 doesn't preserve sign
 */
static int int64_cmp_cast(int64_t x) {
    return  x >> (63 - _lzcnt_u64(x));
}

static int cmp_int64(void* a, void* b) {
    return int64_cmp_cast(*((int64_t*) a) - *((int64_t*) b));
}


#endif

