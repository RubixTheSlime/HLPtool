#include "bitonic_sort.h"
#include "vector_tools.h"

void bitonic_sort4x16x8(uint8_t* arrays) {
    ymm_pair_t pair = {
        _mm256_loadu_si256((__m256i*) arrays),
        _mm256_loadu_si256(((__m256i*) arrays) + 1)
    };
    pair = bitonic_sort4x16x8_inner(pair);
    _mm256_storeu_si256((__m256i*) arrays, pair.ymm0);
    _mm256_storeu_si256(((__m256i*) arrays) + 1, pair.ymm1);
}

