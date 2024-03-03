#ifndef BITONIC_SORT_H
#define BITONIC_SORT_H
#include <stdint.h>
#include <immintrin.h>
#include "vector_tools.h"

#define BITONIC_SORT_BLENDD(pair, imm) \
    pair = (ymm_pair_t) {_mm256_blend_epi32(pair.ymm0, pair.ymm1, (imm << 4) | imm), \
    _mm256_blend_epi32(pair.ymm1, pair.ymm0, (imm << 4) | imm)}

#define BITONIC_SORT_BLENDW(pair) \
    pair = (ymm_pair_t) {_mm256_blend_epi16(pair.ymm0, pair.ymm1, 0b10101010), \
        _mm256_blend_epi16(pair.ymm1, pair.ymm0, 0b10101010)}

#define BITONIC_SORT_SHUFD(pair, imm) \
    pair.ymm1 = _mm256_shuffle_epi32(pair.ymm1, imm)

#define BITONIC_SORT_SHUFB(pair, indices) \
    pair.ymm1 = _mm256_shuffle_epi8(pair.ymm1, indices)

#define BITONIC_SORT_MINMAX(pair) \
    pair = (ymm_pair_t) {_mm256_min_epu8(pair.ymm0, pair.ymm1), \
        _mm256_max_epu8(pair.ymm1, pair.ymm0)}

#define BITONIC_SORT_STEP(pair, blend_imm, shufd_imm) \
    BITONIC_SORT_BLENDD(pair, blend_imm); \
    BITONIC_SORT_SHUFD(pair, shufd_imm); \
    BITONIC_SORT_MINMAX(pair)

static ymm_pair_t bitonic_sort4x16x8_inner(ymm_pair_t pair) {
    // zip together
    pair = (ymm_pair_t) {_mm256_unpackhi_epi8(pair.ymm1, pair.ymm0),
        _mm256_unpacklo_epi8(pair.ymm1, pair.ymm0)};

    // 2
    BITONIC_SORT_MINMAX(pair);

    // 4
    BITONIC_SORT_SHUFD(pair, SHUFD_REV_2x32_256);
    BITONIC_SORT_MINMAX(pair);

    BITONIC_SORT_STEP(pair, 0b1010, SHUFD_REV_2x32_256);

    // 8
    BITONIC_SORT_SHUFD(pair, SHUFD_REV_4x32_256);
    BITONIC_SORT_MINMAX(pair);

    BITONIC_SORT_STEP(pair, 0b1010, SHUFD_REV_2x32_256);
    BITONIC_SORT_STEP(pair, 0b1100, SHUFD_REV_2x64_256);

    // 16
    BITONIC_SORT_SHUFB(pair, SHUFB_REV_8x16_256);
    BITONIC_SORT_MINMAX(pair);
    BITONIC_SORT_BLENDW(pair);
    BITONIC_SORT_SHUFB(pair, SHUFB_REV_4x16_256);

    BITONIC_SORT_STEP(pair, 0b1010, SHUFD_REV_2x32_256);
    BITONIC_SORT_STEP(pair, 0b1100, SHUFD_REV_2x64_256);
    BITONIC_SORT_STEP(pair, 0b1010, SHUFD_REV_2x32_256);

    // pack back together
    // interlace words
    BITONIC_SORT_SHUFB(pair, SHUFB_REV_2x16_256);
    BITONIC_SORT_BLENDW(pair);
    BITONIC_SORT_SHUFB(pair, SHUFB_REV_2x16_256);

    // interlace bytes
    __m256i shifted = _mm256_packus_epi16(_mm256_srli_epi16(pair.ymm0, 8), _mm256_srli_epi16(pair.ymm1, 8));
    __m256i masked = _mm256_packus_epi16(_mm256_and_si256(pair.ymm0, LO_HALVES_8_256), _mm256_and_si256(pair.ymm1, LO_HALVES_8_256));
    return (ymm_pair_t) {shifted, masked};
}

extern void bitonic_sort4x16x8(uint8_t* arrays);

#endif
