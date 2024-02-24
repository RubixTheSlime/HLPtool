#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <immintrin.h>
#include "../aa_tree.h"
#include "HlpSolve.h"
#include <stdbool.h>
#include "../bitonicSort.h"
#include "../vector_tools.h"
#include "../redstone.h"

//#pragma GCC push_options
//#pragma GCC optimize ("O0")

typedef struct branch_layer_s {
    uint64_t map;
    uint16_t configIndex;
    /* uint8_t separations; */
} branch_layer_t;

int solveType;

int cacheSize = 22;

__m256i goalMin;
__m256i goalMax;

int hlpSolveVerbosity = 1;

__m256i dontCareMask;
__m256i dontCarePostSortPerm;
int dontCareCount;

long iter;
int currLayer;
int _uniqueOutputs;
int _solutionsFound;
int _searchAccuracy;
uint16_t* _outputChain;
clock_t programStartT;

int globalMaxDepth;
int globalAccuracy;


int isHex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int toHex(char c) {
    return c - (c <= '9' ? '0' : c <= 'F' ? 'A' - 10 : 'a' - 10);
}

int mapPairContainsRanges(uint64_t mins, uint64_t maxs) {
    while (mins && maxs) {
        int minVal = mins & 15;
        int maxVal = maxs & 15;
        if (minVal != minVal && !(minVal == 0 && maxVal == 15)) return 1;
        mins >>= 4;
        maxs >>= 4;
    }
    return 0;
}

hlp_request_t parseHlpRequestStr(char* str) {
    hlp_request_t result = {0};
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
            if (!isHex(*(c + 2))) {
                result.error = HLP_ERROR_MALFORMED;
                return result;
            }
            result.mins = (result.mins << 4) | toHex(*c);
            result.maxs = (result.maxs << 4) | toHex(*(c + 2));
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
        if (isHex(*c)) {
            result.mins = (result.mins << 4) | toHex(*c);
            result.maxs = (result.maxs << 4) | toHex(*c);
            length++;
            c++;
            continue;
        }
        result.error = HLP_ERROR_MALFORMED;
        return result;
    }
    int remainingLength = 16 - length;
 
    if (remainingLength < 0) {
        result.error = HLP_ERROR_TOO_LONG;
        return result;
    }
    result.mins <<= remainingLength * 4;
    result.maxs <<= remainingLength * 4;
    result.maxs |= ((uint64_t) 1 << (remainingLength * 4)) - 1;

    if (result.mins == result.maxs)
        result.solveType = HLP_SOLVE_TYPE_EXACT;
    else if (mapPairContainsRanges(result.mins, result.maxs))
        result.solveType = HLP_SOLVE_TYPE_RANGED;
    else
        result.solveType = HLP_SOLVE_TYPE_PARTIAL;

    return result;
}


inline ymm_pair_t combineRangesInner(__m256i equalityReference, ymm_pair_t mins, int shift) {
    if (shift > 0) {
        // left shift
        __m256i mask = _mm256_cmpeq_epi8(equalityReference, _mm256_srli_si256(equalityReference, shift));
        mins.ymm0 = _mm256_max_epu8(mins.ymm0, _mm256_slli_si256(_mm256_and_si256(mins.ymm0, mask), shift));
        mins.ymm1 = _mm256_max_epu8(mins.ymm1, _mm256_slli_si256(_mm256_and_si256(mins.ymm1, mask), shift));
    } else {
        // right shift
        __m256i mask = _mm256_cmpeq_epi8(equalityReference, _mm256_slli_si256(equalityReference, -shift));
        mins.ymm0 = _mm256_max_epu8(mins.ymm0, _mm256_srli_si256(_mm256_and_si256(mins.ymm0, mask), -shift));
        mins.ymm1 = _mm256_max_epu8(mins.ymm1, _mm256_srli_si256(_mm256_and_si256(mins.ymm1, mask), -shift));
    }
}

inline ymm_pair_t combineRanges(__m256i equalityReference, ymm_pair_t minsAndMaxs) {
    // we invert the max values so that after shifting things, any zeros
    // shifted in will not affect anything, as we only combine things with max
    // function
    minsAndMaxs.ymm1 = _mm256_xor_si256(minsAndMaxs.ymm1, uint_max256);

    minsAndMaxs = combineRangesInner(equalityReference, minsAndMaxs, 1);
    minsAndMaxs = combineRangesInner(equalityReference, minsAndMaxs, 2);
    minsAndMaxs = combineRangesInner(equalityReference, minsAndMaxs, 4);
    minsAndMaxs = combineRangesInner(equalityReference, minsAndMaxs, 8);

    minsAndMaxs = combineRangesInner(equalityReference, minsAndMaxs, -1);
    minsAndMaxs = combineRangesInner(equalityReference, minsAndMaxs, -2);
    minsAndMaxs = combineRangesInner(equalityReference, minsAndMaxs, -4);
    minsAndMaxs = combineRangesInner(equalityReference, minsAndMaxs, -8);

    minsAndMaxs.ymm1 = _mm256_xor_si256(minsAndMaxs.ymm1, uint_max256);
}

inline int getLegalDistCheckMaskRanged(__m256i sortedYmm, int threshhold) {
    __m256i finalIndices = _mm256_and_si256(sortedYmm, low_halves_mask256);
    __m256i current = _mm256_and_si256(_mm256_srli_epi64(sortedYmm, 4), low_halves_mask256);
    ymm_pair_t final = {_mm256_shuffle_epi8(goalMin, finalIndices), _mm256_shuffle_epi8(goalMax, finalIndices)};
    final = combineRanges(current, final);

    // if the min is higher than the max, that's all we need to know
    __m256i illegals = _mm256_cmpgt_epi8(final.ymm0, final.ymm1);
    int mask = -1;
    mask &= _mm256_testz_si256(split_test_mask256, illegals) | (_mm256_testc_si256(split_test_mask256, illegals) << 2);

    __m256i finalDelta = _mm256_max_epi8(
            _mm256_sub_epi8(final.ymm0, _mm256_srli_si256(final.ymm1, 1)),
            _mm256_sub_epi8(_mm256_srli_si256(final.ymm0, 1), final.ymm1)
            );
    __m256i currentDelta = _mm256_abs_epi8(_mm256_sub_epi8(_mm256_srli_si256(current, 1), current));

    uint32_t separationsMask = _mm256_movemask_epi8(_mm256_cmpgt_epi8(finalDelta, currentDelta)) & 0x7fff7fff;
    mask &= (_popcnt32(separationsMask & 0xffff) <= threshhold) | ((_popcnt32(separationsMask >> 16) <= threshhold) << 2);
    return mask;
}

inline int getLegalDistCheckMaskPartial(__m256i sortedYmm, int threshhold) {
    __m256i final = _mm256_and_si256(sortedYmm, low_halves_mask256);
    __m256i current = _mm256_and_si256(_mm256_srli_epi64(sortedYmm, 4), low_halves_mask256);

    __m256i finalDelta = _mm256_abs_epi8(_mm256_sub_epi8(_mm256_srli_si256(final, 1), final));
    __m256i currentDelta = _mm256_abs_epi8(_mm256_sub_epi8(_mm256_srli_si256(current, 1), current));

    __m256i illegals = _mm256_and_si256(_mm256_cmpeq_epi8(currentDelta, _mm256_setzero_si256()), finalDelta);
    illegals = _mm256_and_si256(illegals, low_15_bytes_mask256);
    int mask = -1;
    mask &= _mm256_testz_si256(split_test_mask256, illegals) | (_mm256_testc_si256(split_test_mask256, illegals) << 2);

    uint32_t separationsMask = _mm256_movemask_epi8(_mm256_cmpgt_epi8(finalDelta, currentDelta)) & 0x7fff7fff;
    mask &= (_popcnt32(separationsMask & 0xffff) <= threshhold) | ((_popcnt32(separationsMask >> 16) <= threshhold) << 2);
    return mask;
}

int batchApplyAndCheckExact(
        uint64_t input,
        uint64_t* maps,
        branch_layer_t* outputs,
        int quantity,
        int threshhold,
        const int variant) {
    __m256i doubledInput = _mm256_permute4x64_epi64(_mm256_castsi128_si256(unpack_uint_to_xmm(input)), 0x44);

    // this contains extra bits to overwrite the current value on dont care entries
    __m256i doubledGoal;
    if (variant == HLP_SOLVE_TYPE_RANGED) 
        doubledGoal = identity_permutation256;
    else
        doubledGoal = _mm256_or_si256(goalMin, dontCareMask);

    branch_layer_t* currentOutput = outputs;

    for (int i = (quantity - 1) / 4; i >= 0; i--) {
        ymm_pair_t quad = quad_unpack_map256(_mm256_loadu_si256(((__m256i*) maps) + i));
        quad.ymm0 = _mm256_shuffle_epi8(quad.ymm0, doubledInput);
        quad.ymm1 = _mm256_shuffle_epi8(quad.ymm1, doubledInput);

        ymm_pair_t sortedQuad = { _mm256_or_si256(doubledGoal, _mm256_slli_epi64(quad.ymm0, 4)),
            _mm256_or_si256(doubledGoal, _mm256_slli_epi64(quad.ymm1, 4)) };
        sortedQuad = bitonic_sort4x16x8_inner(sortedQuad);

        sortedQuad.ymm0 = _mm256_shuffle_epi8(sortedQuad.ymm0, dontCarePostSortPerm);
        sortedQuad.ymm1 = _mm256_shuffle_epi8(sortedQuad.ymm1, dontCarePostSortPerm);
        int mask;
        if (variant == HLP_SOLVE_TYPE_RANGED)
            mask = getLegalDistCheckMaskRanged(sortedQuad.ymm0, threshhold) | (getLegalDistCheckMaskRanged(sortedQuad.ymm1, threshhold) << 1);
        else
            mask = getLegalDistCheckMaskPartial(sortedQuad.ymm0, threshhold) | (getLegalDistCheckMaskPartial(sortedQuad.ymm1, threshhold) << 1);
        if (i & (mask == 0)) continue;
        __m256i packed = quad_pack_map256(quad);

        // can't just use a for loop because const variables aren't const enough
        currentOutput->configIndex = i * 4 + 3;
        currentOutput->map = _mm256_extract_epi64(packed, 3);
        currentOutput += (mask >> 3) & 1;

        currentOutput->configIndex = i * 4 + 2;
        currentOutput->map = _mm256_extract_epi64(packed, 2);
        currentOutput += (mask >> 2) & 1;

        currentOutput->configIndex = i * 4 + 1;
        currentOutput->map = _mm256_extract_epi64(packed, 1);
        currentOutput += (mask >> 1) & 1;

        currentOutput->configIndex = i * 4;
        currentOutput->map = _mm256_extract_epi64(packed, 0);
        currentOutput += mask & 1;
    }

    return currentOutput - outputs;
}

int getMinGroup(uint64_t mins, uint64_t maxs) {
    // not great but works for now
    uint16_t bitFeild = 0;
    for(int i = 16; i; i--) {
        if ((mins & 15) == (maxs & 15))
            bitFeild |= 1 << (mins & 15);
        mins >>= 4;
        maxs >>= 4;
    }
    int result = _popcnt32(bitFeild);
    if (!result) return 1;
    return result;
}

//faster implementation of searching over the last layer while checking if you found the goal, unexpectedly big optimization
int fastLastLayerSearch(uint64_t input, struct precomputed_hex_layer* layer, const int variant) {
    __m256i doubledInput = _mm256_permute4x64_epi64(_mm256_castsi128_si256(unpack_uint_to_xmm(input)), 0x44);

    __m256i* quadMaps = (__m256i*) (layer->next_layer_luts);

    iter += layer->next_layer_count;
    for (int i = (layer->next_layer_count - 1) / 4; i >= 0; i--) {
        ymm_pair_t quad = quad_unpack_map256(_mm256_loadu_si256(quadMaps + i));

        // determine if there are any spots that do not match up
        // if all zeros, that means they match
        // instead of split tests based on variant, the ranged test is only 2
        // cycles longer than the exact mode, so we just use ranged for
        // everything to avoid branches
        quad.ymm0 = _mm256_shuffle_epi8(quad.ymm0, doubledInput);
        quad.ymm1 = _mm256_shuffle_epi8(quad.ymm1, doubledInput);
        quad.ymm0 = _mm256_or_si256(_mm256_cmpgt_epi8(goalMin, quad.ymm0), _mm256_cmpgt_epi8(quad.ymm0, goalMax));
        quad.ymm1 = _mm256_or_si256(_mm256_cmpgt_epi8(goalMin, quad.ymm1), _mm256_cmpgt_epi8(quad.ymm1, goalMax));
        // no need to apply dontCareMask, they already will always succeed anyways
        if (i && _mm256_testnzc_si256(split_test_mask256, quad.ymm0) && _mm256_testnzc_si256(split_test_mask256, quad.ymm1)) continue;

        bool successes[] = {
            _mm256_testz_si256(split_test_mask256, quad.ymm0),
            _mm256_testz_si256(split_test_mask256, quad.ymm1),
            _mm256_testc_si256(split_test_mask256, quad.ymm0),
            _mm256_testc_si256(split_test_mask256, quad.ymm1)};

        for (int j=0; j<4; j++) {
            if (!successes[j]) continue;
            int index = i * 4 + j;
            iter -= index;
            uint16_t config = layer->next_layers[index]->config;
            if (_solutionsFound != -1) {
                _solutionsFound++;
                continue;
            }
            if (_outputChain != 0) _outputChain[currLayer - 1] = config;
            return 1;
        }
    }
    return 0;
}

//cache related code, used for removing identical or worse solutions
long sameDepthHits = 0;
long difLayerHits = 0;
long misses = 0;
long bucketUtil = 0;
long cacheChecksTotal = 0;

typedef struct cache_entry_s {
    uint64_t map;
    uint32_t trial;
    uint8_t depth;
} cache_entry_t;

cache_entry_t* cacheArr;
uint64_t cacheMask;
uint32_t cacheTrialGlobal = 0;

void clearCache() {
    for (int i = 0; i< (1<<cacheSize); i++) {
        cacheArr[i].map = 0;
        cacheArr[i].depth = 0;
        cacheArr[i].trial = 0;
    }
}

int cacheCheck(uint64_t output, int depth) {
    uint32_t pos = _mm_crc32_u32(_mm_crc32_u32(0, output & UINT32_MAX), output >> 32) & cacheMask;
    cache_entry_t* entry = cacheArr + pos;
    cacheChecksTotal++;
    if (entry->map == output && entry->depth <= depth && entry->trial == cacheTrialGlobal) {
        if (entry->depth == depth) sameDepthHits++;
        else difLayerHits++;
        return 1;
    }

    if (entry->trial == cacheTrialGlobal && entry->map != output) misses++;
    else bucketUtil++;


    entry->map = output;
    entry->depth = depth;
    entry->trial = cacheTrialGlobal;

    return 0;
}

void invalidateCache() {
    cacheTrialGlobal++;
    if (!cacheTrialGlobal) {
        clearCache();
        // trial 0 should always mean blank
        cacheTrialGlobal++;
    }
}

// the most number of separations that can be found in the distance check before it prunes
int getDistThreshold(int remainingLayers) {
    if (_searchAccuracy == ACCURACY_REDUCED) return remainingLayers - (remainingLayers > 2);
    // n is always sufficient anyways for 15-16 outputs
    if (_searchAccuracy == ACCURACY_NORMAL || _uniqueOutputs > 14) return remainingLayers;
    // n+1 is always sufficient for 14 outputs
    if (_searchAccuracy == ACCURACY_INCREASED || _uniqueOutputs > 13) return remainingLayers + 1;

    // currently the best known general threshhold
    // +/-1 is for round up division
    return ((remainingLayers * 3 - 1) >> 1) + 1;
}

/* test to see if this map falls under a solution
 */
int testMap(uint64_t map) {
    __m128i xmm = unpack_uint_to_xmm(map);
    return _mm_testz_si128(_mm_or_si128(
                _mm_cmpgt_epi8(_mm256_castsi256_si128(goalMin), xmm),
                _mm_cmpgt_epi8(xmm, _mm256_castsi256_si128(goalMax))
                ), uint_max128);
}

branch_layer_t potentialLayers[800*32];

//main dfs recursive search function
int dfs(uint64_t input, int depth, struct precomputed_hex_layer* layer) {
    // test to see if we found a solution, even if we're not at the end. this
    // can happen even though it seems like it shouldn't
    if (testMap(input)) {
        currLayer = depth + 1;
        if (_outputChain != 0) _outputChain[depth] = layer->config;
        return 1;
    }

    if(depth == currLayer - 1) return fastLastLayerSearch(input, layer, solveType);
    iter += layer->next_layer_count;
    int totalNextLayersIdentified = batchApplyAndCheckExact(
            input,
            layer->next_layer_luts,
            potentialLayers + 800*depth,
            layer->next_layer_count,
            getDistThreshold(currLayer - depth - 1),
            solveType
            );

    for(int i = totalNextLayersIdentified - 1; i >= 0; i--) {
        branch_layer_t* entry = potentialLayers + 800*depth + i;
        int conf = entry->configIndex;
        struct precomputed_hex_layer* next_layer = layer->next_layers[conf];
        /* uint64_t output = apply_mapping_packed64(input, layer->next_layer_luts[conf]); */
        uint64_t output = apply_mapping_packed64(input, next_layer->map);
        /* uint64_t output = entry->map; */
        /* uint64_t output = hex_layer64(input, next_layer->config); */

        //cache check
        if(cacheCheck(output, depth)) continue;

        //call next layers
        if(dfs(output, depth + 1, next_layer)) {
            if (_outputChain != 0) _outputChain[depth] = next_layer->config;
            return 1;
        }
        if (hlpSolveVerbosity < 3) continue;
        if(depth == 0 && currLayer > 8) printf("done:%d/%d\n", conf, layer->next_layer_count);
    }
    return 0;
}

int init(hlp_request_t request) {
    programStartT = clock();
    if (!cacheArr) cacheArr = calloc((1 << cacheSize), sizeof(cache_entry_t));
    cacheMask = (1 << cacheSize) - 1;
    iter = 0;
    cacheChecksTotal = 0;

    solveType = request.solveType;

    switch (solveType) {
        case HLP_SOLVE_TYPE_EXACT:
            _uniqueOutputs = get_group64(request.mins);
            break;
        case HLP_SOLVE_TYPE_PARTIAL:
            _uniqueOutputs = getMinGroup(request.mins, request.maxs);
            break;
        default:
            printf("you found a search mode that isn't implemented\n");
            return 1;
    }

    
    goalMin = _mm256_permute4x64_epi64(_mm256_castsi128_si256(big_endian_uint_to_xmm(request.mins)), 0x44);
    goalMax = _mm256_permute4x64_epi64(_mm256_castsi128_si256(big_endian_uint_to_xmm(request.maxs)), 0x44);

    dontCareMask = _mm256_cmpeq_epi8(_mm256_sub_epi8(goalMax, goalMin), low_halves_mask256);
    dontCareCount = _popcnt32(_mm_movemask_epi8(_mm256_castsi256_si128(dontCareMask)));
    dontCarePostSortPerm = _mm256_min_epi8(identity_permutation256, _mm256_set1_epi8(15 - dontCareCount));

    return 0;
}

//main search loop
int singleSearchInner(struct precomputed_hex_layer* base_layer, int maxDepth) {
    currLayer = 1;

    while (currLayer <= maxDepth) {
        if(dfs(identity_permutation_packed64, 0, base_layer)) {
            if (hlpSolveVerbosity >= 3) {
                printf("solution found at %.2fms\n", (double)(clock() - programStartT) / CLOCKS_PER_SEC * 1000);
                printf("total iter over all: %'ld\n", iter);
                printf("cache checks: %'ld; same depth hits: %'ld; dif layer hits: %'ld; misses: %'ld; bucket utilization: %'ld\n", cacheChecksTotal, sameDepthHits, difLayerHits, misses, bucketUtil);
            }
            return currLayer;
        }
        invalidateCache();
        currLayer++;

        if (hlpSolveVerbosity < 2) continue;
        printf("search over layer %d done\n",currLayer - 1);

        if (hlpSolveVerbosity < 3) continue;
        printf("layer search done after %.2fms; %'ld iterations\n", (double)(clock() - programStartT) / CLOCKS_PER_SEC * 1000, iter);
    }
    if (hlpSolveVerbosity >= 2) {
        printf("failed to beat depth\n");
        printf("cache checks: %'ld; same depth hits: %'ld; dif layer hits: %'ld; misses: %'ld; bucket utilization: %'ld\n", cacheChecksTotal, sameDepthHits, difLayerHits, misses, bucketUtil);
    }
    return maxDepth + 1;
}

int singleSearch(hlp_request_t request, uint16_t* outputChain, int maxDepth, enum SearchAccuracy accuracy) {
    if (maxDepth < 0 || maxDepth > 31) maxDepth = 31;
    if (request.mins == 0) {
        if (outputChain) outputChain[0] = 0x2f0;
        return 1;
    }

    if (init(request)) return -1;

    _outputChain = outputChain;
    _solutionsFound = -1;
    _searchAccuracy = accuracy;
    struct precomputed_hex_layer* identity_layer = precompute_hex_layers(_uniqueOutputs);
    return singleSearchInner(identity_layer, maxDepth);
}

int solve(hlp_request_t request, uint16_t* outputChain, int maxDepth, enum SearchAccuracy accuracy) {
    int requestedMaxDepth = maxDepth;
    if (maxDepth < 0 || maxDepth > 31) maxDepth = 31;

    if (init(request)) {
        printf("an error occurred\n");
        return requestedMaxDepth + 1;
    }

    struct precomputed_hex_layer* identity_layer = precompute_hex_layers(_uniqueOutputs);
    /* return requestedMaxDepth + 1; */

    if (request.mins == 0) {
        if (outputChain) outputChain[0] = 0x2f0;
        return 1;
    }

    _outputChain = outputChain;
    _solutionsFound = -1;
    int solutionLength = maxDepth;

    if (hlpSolveVerbosity >= 2) {
        if (accuracy > ACCURACY_REDUCED) printf("starting presearch\n");
        else printf("starting search\n");
    }

    // reduced accuracy search is sometimes faster than the others but
    // still often gets an optimal solution, so we start with that so the
    // "real" search can cut short if it doesn't find a better solution.
    // when it's not faster, the solution is found pretty fast anyways.
    _searchAccuracy = ACCURACY_REDUCED;
    solutionLength = singleSearchInner(identity_layer, solutionLength);

    if (solutionLength == maxDepth) solutionLength = maxDepth;
    if (accuracy == ACCURACY_REDUCED) return solutionLength;
    long totalIter = iter;
    iter = 0;

    if (hlpSolveVerbosity >= 2) printf("starting main search\n");

    _searchAccuracy = accuracy;
    int result = singleSearchInner(identity_layer, solutionLength - 1);
    if (hlpSolveVerbosity >= 2) printf("total iter across searches: %'ld\n", totalIter + iter);
    if (result > maxDepth) return requestedMaxDepth + 1;
    return result;
}

void hlpSetCacheSize(int size) {
    if (cacheArr) {
        free(cacheArr);
        cacheArr = 0;
    }
    cacheSize = size;
    cacheMask = (1 << size) - 1;
}

uint64_t applyChain(uint64_t start, uint16_t* chain, int length) {
    for (int i = 0; i < length; i++) {
        start = hex_layer64(start, chain[i]);
    }
    return start;
}

void printChain(uint16_t* chain, int length) {
    const char layerStrings[][16] = {
        "%X, %X",
        "%X, *%X",
        "*%X, %X",
        "*%X, *%X",
        "^%X, *%X",
        "^*%X, %X"
    };
    for (int i = 0; i < length; i++) {
        uint16_t conf = chain[i];
        printf(layerStrings[conf >> 8], (conf >> 4) & 15, conf & 15);
        if (i < length - 1) printf(";  ");
    }
}

void printHlpMap(uint64_t map) {
    hlp_request_t request = {map, map};
    printHlpRequest(request);
}

void printHlpRequest(hlp_request_t request) {
    for (int i = 15; i >= 0; i--) {
        if (i % 4 == 3 && i != 15) printf(" ");
        int minVal = (request.mins >> i * 4) & 15;
        int maxVal = (request.maxs >> i * 4) & 15;
        if (minVal == maxVal) {
            printf("%X", minVal);
            continue;
        }
        if (minVal == 0 && maxVal == 15) {
            printf("X");
            continue;
        }
        printf("[%X-%X]", minVal, maxVal);
    }
}


void hlpPrintSearch(char* map) {
    uint16_t result[32];
    hlp_request_t request = parseHlpRequestStr(map);
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

    if (hlpSolveVerbosity > 0) {
        printf("searching for ");
        printHlpRequest(request);
        printf("\n");
    }

    int length = solve(request, result, globalMaxDepth, globalAccuracy);

    if (length > globalMaxDepth) {
        if (hlpSolveVerbosity > 0)
            printf("no result found\n");
    } else {
        if (hlpSolveVerbosity > 0) {
            printf("result found, length %d", length);
            if (hlpSolveVerbosity > 2 || request.solveType != HLP_SOLVE_TYPE_EXACT) {
                printf(" (");
                printHlpMap(applyChain(identity_permutation_big_endian64, result, length));
                printf(")");
            }
            printf(":  ");
        }
        printChain(result, length);
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
    { "max-length", LONG_OPTION_MAX_DEPTH, "N", 0, "Limit results to chains up to N layers long" },
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
                globalAccuracy = level;
            break;
        case 'f':
            return parse_opt(LONG_OPTION_ACCURACY, "-1", state);
        case 'p':
            return parse_opt(LONG_OPTION_ACCURACY, "2", state);
        case LONG_OPTION_MAX_DEPTH:
            globalMaxDepth = atoi(arg);
            break;
        case LONG_OPTION_CACHE_SIZE:
            hlpSetCacheSize(atoi(arg) - 4);
            break;
        case ARGP_KEY_INIT:
            globalAccuracy = ACCURACY_NORMAL;
            globalMaxDepth = 31;
            settings->settings_redstone.global = settings->global;
            state->child_inputs[0] = &settings->settings_redstone;
            break;
        case ARGP_KEY_SUCCESS:
            hlpSolveVerbosity = settings->global->verbosity;
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
