#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <emmintrin.h>
/* #include <intrin.h> */
#include "aa_tree.h"
#include "HlpSolve.h"
#include <stdbool.h>
#include "bitonicSort.h"

//#pragma GCC push_options
//#pragma GCC optimize ("O0")

typedef struct branch_layer_s {
    uint64_t map;
    uint16_t configIndex;
    /* uint8_t separations; */
} branch_layer_t;

int solveType;

const uint64_t hlpStartPos = 0x0123456789abcdef;
const uint64_t startPos = 0x7f6e5d4c3b2a1908;
const uint64_t broadcastH16 = 0x1111111111111111;
int cacheSize = 22;

__m128i goalMin;
__m128i goalMax;

//precomputed layer lookup tables
uint16_t* layerConf;
uint16_t* nextValidLayers;
uint64_t* nextValidLayerLuts;
int* nextValidLayersSize;
int hlpSolveVerbosity = 1;


uint16_t* layerConfAll[16] = {0};
uint16_t* nextValidLayersAll[16] = {0};
uint64_t* nextValidLayerLutsAll[16] = {0};
int* nextValidLayersSizeAll[16] = {0};
uint16_t layerPrecomputesFinished = 0;

uint16_t getLayerConf(int group, int layerId) { return layerConfAll[group - 1][layerId];}
uint16_t getNextValidLayerId(int group, int prevLayerId, int index) { return nextValidLayersAll[group - 1][800 * prevLayerId + index]; }
uint16_t getNextValidLayerSize(int group, int layerId) { return nextValidLayersSizeAll[group - 1][layerId]; }

long iter;
int currLayer;
int _uniqueOutputs;
int _solutionsFound;
int _searchAccuracy;
uint16_t* _outputChain;
clock_t programStartT;

const uint64_t lowHalvesMask64 = 0x0f0f0f0f0f0f0f0f;
const __m128i lowHalvesMask128 = {lowHalvesMask64, lowHalvesMask64};
const __m256i lowHalvesMask256 = {lowHalvesMask64, lowHalvesMask64, lowHalvesMask64, lowHalvesMask64};

__m256i lowHalvesMask256_2() {
    // todo: find a way to do this that gcc doesn't deoptimize
    __m256i ones;
    ones = _mm256_cmpeq_epi32(ones, ones);
    ones = _mm256_srli_epi16(ones, 12);
    ones = _mm256_packus_epi16(ones, ones);
    return ones;
}

const __m128i fixUintPerm = {0x0c040d050e060f07, 0x080009010a020b03};

int isHex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int toHex(char c) {
    return c - (c <= '9' ? '0' : c <= 'A' ? 'A' - 10 : 'a' - 10);
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
        if (*c == '.') {
            result.mins = (result.mins << 4) | 0;
            result.maxs = (result.maxs << 4) | 15;
            length++;
            c++;
            continue;
        }
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
 
    if (length > 16) {
        result.error = HLP_ERROR_TOO_LONG;
        return result;
    }
    result.mins <<= (16 - length) * 4;
    result.maxs <<= (16 - length) * 4;
    result.maxs |= ((1 << ((16 - length) * 4)) - 1);

    if (result.mins == result.maxs)
        result.solveType = HLP_SOLVE_TYPE_EXACT;
    else
        result.solveType = HLP_SOLVE_TYPE_RANGED;

    return result;
}

__m128i uintToXmm(uint64_t uint) {
    __m128i input = _mm_cvtsi64_si128(uint);
    return _mm_and_si128(_mm_or_si128(_mm_slli_si128(input, 8), _mm_srli_epi64(input, 4)), lowHalvesMask128);
}

uint64_t xmmToUint(__m128i xmm) {
    return _mm_cvtsi128_si64(_mm_or_si128(_mm_slli_epi32(xmm, 4), _mm_srli_si128(xmm, 8)));
}

// converts a "nice" uint64 into one that works properly for applying precomputed maps
__m128i prettyUintToXmm(uint64_t uint) {
    return _mm_shuffle_epi8(uintToXmm(uint), fixUintPerm);
}

// assumes both are in the "fixed" format
uint64_t apply_mapping(uint64_t input, uint64_t map) {
    return xmmToUint(_mm_shuffle_epi8(uintToXmm(map), uintToXmm(input)));
}

uint64_t layer(uint64_t start, uint16_t config) {
    // adjust mode if rotated
    // this makes it so the three bits in upper byte of config are independent
    config += (config & 0x400) >> 2;

    // unpack map and config
    __m128i input = uintToXmm(start);
    __m128i back1 = input;
    __m128i side2 = input;

    __m128i v_config = _mm_cvtsi32_si128(config);
    __m128i back2 = _mm_broadcastb_epi8(v_config);
    __m128i side1 = _mm_and_si128(_mm_srli_epi64(back2, 4), lowHalvesMask128);
    back2 = _mm_and_si128(back2, lowHalvesMask128);

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
    return xmmToUint(output);
}

ymm_pair_t quadUnpackMap(__m256i packed) {
    const __m256i upackShifts = {4, 0, 4, 0};
    ymm_pair_t pair = {
        _mm256_and_si256(_mm256_srlv_epi64(_mm256_shuffle_epi32(packed, 0x44), upackShifts), lowHalvesMask256),
        _mm256_and_si256(_mm256_srlv_epi64(_mm256_shuffle_epi32(packed, 0xee), upackShifts), lowHalvesMask256)};
    return pair;
}

__m256i quadPackMap(ymm_pair_t unpacked) {
    return _mm256_blend_epi32(
            _mm256_or_si256(_mm256_srli_si256(unpacked.ymm0, 8), _mm256_slli_epi64(unpacked.ymm0, 4)),
            _mm256_or_si256(unpacked.ymm1, _mm256_slli_epi64(_mm256_slli_si256(unpacked.ymm1, 8), 4)),
            0b11001100
            );
}

inline __m256i quickGetTestMask() {
    __m128i dummy;
    // fortunately, gcc somehow does not deoptimize this
    return _mm256_castsi128_si256(_mm_cmpeq_epi64(dummy, dummy));
}

inline __m256i quickGetLow15Mask() {
    __m256i ymm = {-1,-1,-1,-1};
    return _mm256_srli_si256(ymm, 1);
}

inline int getLegalDistCheckMaskExact(__m256i sortedYmm, int threshhold) {
    const __m256i splitTestMask = quickGetTestMask();
    const __m256i low15Mask = quickGetLow15Mask();
    __m256i final = _mm256_and_si256(sortedYmm, lowHalvesMask256);
    __m256i current = _mm256_and_si256(_mm256_srli_epi64(sortedYmm, 4), lowHalvesMask256);

    __m256i finalDelta = _mm256_abs_epi8(_mm256_sub_epi8(_mm256_srli_si256(final, 1), final));
    __m256i currentDelta = _mm256_abs_epi8(_mm256_sub_epi8(_mm256_srli_si256(current, 1), current));

    __m256i illegals = _mm256_and_si256(_mm256_cmpeq_epi8(currentDelta, _mm256_setzero_si256()), finalDelta);
    illegals = _mm256_and_si256(illegals, low15Mask);
    int mask = -1;
    mask &= _mm256_testz_si256(splitTestMask, illegals) | (_mm256_testc_si256(splitTestMask, illegals) << 2);

    uint32_t separationsMask = _mm256_movemask_epi8(_mm256_cmpgt_epi8(finalDelta, currentDelta)) & 0x7fff7fff;
    mask &= (_popcnt32(separationsMask & 0xffff) <= threshhold) | ((_popcnt32(separationsMask >> 16) <= threshhold) << 2);
    return mask;
}

int batchApplyAndCheckExact(
        uint64_t input,
        uint64_t* maps,
        branch_layer_t* outputs,
        int quantity,
        int threshhold) {
    __m256i doubledInput = _mm256_permute4x64_epi64(_mm256_castsi128_si256(uintToXmm(input)), 0x44);
    __m256i doubledGoal = _mm256_permute4x64_epi64(_mm256_castsi128_si256(goalMin), 0x44);

    branch_layer_t* currentOutput = outputs;

    for (int i = (quantity - 1) / 4; i >= 0; i--) {
        ymm_pair_t quad = quadUnpackMap(_mm256_loadu_si256(((__m256i*) maps) + i));
        quad.ymm0 = _mm256_shuffle_epi8(quad.ymm0, doubledInput);
        quad.ymm1 = _mm256_shuffle_epi8(quad.ymm1, doubledInput);

        ymm_pair_t sortedQuad = { _mm256_or_si256(doubledGoal, _mm256_slli_epi64(quad.ymm0, 4)),
            _mm256_or_si256(doubledGoal, _mm256_slli_epi64(quad.ymm1, 4)) };
        sortedQuad = bitonic_sort4x16x8_inner(sortedQuad);
        int mask = getLegalDistCheckMaskExact(sortedQuad.ymm0, threshhold) | (getLegalDistCheckMaskExact(sortedQuad.ymm1, threshhold) << 1);
        if (i & (mask == 0)) continue;
        __m256i packed = quadPackMap(quad);

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

//counts uniqe values, usefull for generalizable prune of layers that reduce too much from the get go
int getGroup(uint64_t x) {
    uint16_t bitFeild = 0;
    for(int i = 0; i < 16; i++) {
        int ss = (x >> (i * 4)) & 15;
        bitFeild |= 1 << ss;
    }
    return _popcnt32(bitFeild);
}

//precompute of layers into lut, proceding layers deduplicated for lower branching, and distance estimate table
void precomputeLayers(int group) {
    if ((layerPrecomputesFinished >> (group - 1)) & 1) {
        layerConf = layerConfAll[group-1];
        nextValidLayers = nextValidLayersAll[group-1];
        nextValidLayerLuts = nextValidLayerLutsAll[group-1];
        nextValidLayersSize = nextValidLayersSizeAll[group-1];
        return;
    }

    int layerCount = 0;
    layerConf = malloc(800*sizeof(uint16_t));
    nextValidLayers = malloc(800*800*sizeof(uint16_t));
    nextValidLayerLuts = calloc(800*800, sizeof(uint64_t));
    nextValidLayersSize = malloc(800*sizeof(int));

    int64_t flag;
    aatree_node* uniqueNextLayersTree = aa_tree_insert(startPos, NULL, &flag);
    /* uint64_t* uniqueNextLayersList = calloc(800*800, sizeof(uint64_t)); */
    /* for (int i=0; i<800*800; i++) uniqueNextLayersList[i] = 0; */

    if (hlpSolveVerbosity >= 3) printf("starting layer precompute\n");
    for(int conf = 0; conf < 1536; conf++) {
        uint64_t output = layer(startPos, conf);
        if(getGroup(output) < group) continue;
        if (output == startPos) continue;
        if (aa_tree_search(uniqueNextLayersTree, output)) continue;
        uniqueNextLayersTree = aa_tree_insert(output, uniqueNextLayersTree, &flag);
        /* if (inList(output, uniqueNextLayersList, layerCount)) continue; */
        /* uniqueNextLayersList[layerCount] = output; */

        layerConf[layerCount] = conf;
        nextValidLayers[799 * 800 + layerCount] = layerCount;
        nextValidLayerLuts[799 * 800 + layerCount] = output;
        layerCount++;
    }
    nextValidLayersSize[799] = layerCount;
    long totalNext = layerCount;
    if (hlpSolveVerbosity >= 3) printf("starting next layer precompute\n");
    for(int conf = 0; conf < layerCount; conf++) {
        uint64_t layerOut = nextValidLayerLuts[799*800 + conf];
        int nextLayerSize = 0;
        for (int conf2 = 0; conf2 < layerCount; conf2++) {
            uint64_t nextLayerOut = apply_mapping(layerOut, nextValidLayerLuts[799*800 + conf2]);
            if(getGroup(nextLayerOut) < group) continue;
            if (nextLayerOut == startPos) continue;

            if (aa_tree_search(uniqueNextLayersTree, nextLayerOut)) continue;
            uniqueNextLayersTree = aa_tree_insert(nextLayerOut, uniqueNextLayersTree, &flag);
            /* if (inList(nextLayerOut, uniqueNextLayersList, totalNext)) continue; */
            /* uniqueNextLayersList[totalNext] = nextLayerOut; */

            nextValidLayers[conf * 800 + nextLayerSize] = conf2;
            nextValidLayerLuts[conf * 800 + nextLayerSize] = nextValidLayerLuts[799*800 + conf2];
            totalNext++;
            nextLayerSize++;
        }
        for (int i=0; i<4; i++)
            nextValidLayerLuts[conf*800 + nextLayerSize + i] = 0;
        nextValidLayersSize[conf] = nextLayerSize;
    }

    aa_tree_free(uniqueNextLayersTree);
    /* free(uniqueNextLayersList); */

    layerConfAll[group-1] = layerConf;
    nextValidLayersAll[group-1] = nextValidLayers;
    nextValidLayerLutsAll[group-1] = nextValidLayerLuts;
    nextValidLayersSizeAll[group-1] = nextValidLayersSize;
    layerPrecomputesFinished |= 1 << (group -1);

    if (hlpSolveVerbosity < 3) return;

    printf("layer precompute done at %.2fms\n", (double)(clock() - programStartT) / CLOCKS_PER_SEC * 1000);
    printf("layers computed:%d, total next layers:%'ld\n", layerCount, totalNext - layerCount);
}

//faster implementation of searching over the last layer while checking if you found the goal, unexpectedly big optimization
int fastLastLayerSearchExact(uint64_t input, int prevLayerConf) {
    __m256i doubledInput = _mm256_permute4x64_epi64(_mm256_castsi128_si256(uintToXmm(input)), 0x44);
    __m256i doubledGoal = _mm256_permute4x64_epi64(_mm256_castsi128_si256(goalMin), 0x44);

    __m256i* quadMaps = (__m256i*) (nextValidLayerLuts + 800*prevLayerConf);

    iter += nextValidLayersSize[prevLayerConf];
    const __m256i splitTestMask = quickGetTestMask();
    for (int i = (nextValidLayersSize[prevLayerConf] - 1) / 4; i >= 0; i--) {
        ymm_pair_t quad = quadUnpackMap(_mm256_loadu_si256(quadMaps + i));
        quad.ymm0 = _mm256_xor_si256(_mm256_shuffle_epi8(quad.ymm0, doubledInput), doubledGoal);
        quad.ymm1 = _mm256_xor_si256(_mm256_shuffle_epi8(quad.ymm1, doubledInput), doubledGoal);
        if (i && _mm256_testnzc_si256(splitTestMask, quad.ymm0) && _mm256_testnzc_si256(splitTestMask, quad.ymm1)) continue;

        bool successes[] = {
            _mm256_testz_si256(splitTestMask, quad.ymm0),
            _mm256_testz_si256(splitTestMask, quad.ymm1),
            _mm256_testc_si256(splitTestMask, quad.ymm0),
            _mm256_testc_si256(splitTestMask, quad.ymm1)};

        for (int j=0; j<4; j++) {
            if (!successes[j]) continue;
            int index = i * 4 + j;
            iter -= index;
            uint16_t config = layerConf[nextValidLayers[prevLayerConf * 800 + index]];
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

/*
int cmpfunc (const void * a, const void * b) {
   return ( ((branch_layer_t*)a)->separations - ((branch_layer_t*)b)->separations );
}*/



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

branch_layer_t potentialLayers[800*32];

//main dfs recursive search function
int dfs(uint64_t input, int depth, int prevLayerConf) {
    if(depth == currLayer - 1) return fastLastLayerSearchExact(input, prevLayerConf);
    iter += nextValidLayersSize[prevLayerConf];

    int totalNextLayersIdentified = batchApplyAndCheckExact(
            input,
            nextValidLayerLuts + prevLayerConf*800,
            potentialLayers + 800*depth,
            nextValidLayersSize[prevLayerConf],
            getDistThreshold(currLayer - depth - 1)
            );

    // this adds a very slight boost by checking more promising branches first
    /* if (depth < 4) */
        /* qsort(potentialLayers + 800*depth, totalNextLayersIdentified, sizeof(branch_layer_t), cmpfunc); */

    for(int i = totalNextLayersIdentified - 1; i >= 0; i--) {
        branch_layer_t* entry = potentialLayers + 800*depth + i;
        int conf = entry->configIndex;
        uint64_t output = entry->map;
        // uint64_t output = layer(input, layerConf[nextValidLayers[prevLayerConf * 800 + conf]]);

        //cache check
        if(cacheCheck(output, depth)) continue;

        int index = nextValidLayers[prevLayerConf * 800 + conf];
        //call next layers
        if(dfs(output, depth + 1, index)) {
            if (_outputChain != 0) _outputChain[depth] = layerConf[index];
            return 1;
        }
        if (hlpSolveVerbosity < 3) continue;
        if(depth == 0 && currLayer > 8) printf("done:%d/%d\n", conf, nextValidLayersSize[prevLayerConf]);
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

    if (solveType == HLP_SOLVE_TYPE_EXACT) {
        _uniqueOutputs = getGroup(request.mins);
    } else {
        printf("you found a search mode that isn't implemented\n");
        return 1;
    }

    precomputeLayers(_uniqueOutputs);
    
    goalMin = prettyUintToXmm(request.mins);
    goalMax = prettyUintToXmm(request.maxs);
    return 0;
}

//main search loop
int singleSearchInner(int maxDepth) {
    currLayer = 1;

    while (currLayer <= maxDepth) {
        if(dfs(startPos, 0, 799)) {
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
    return singleSearchInner(maxDepth);
}

int solve(hlp_request_t request, uint16_t* outputChain, int maxDepth, enum SearchAccuracy accuracy) {
    if (maxDepth < 0 || maxDepth > 31) maxDepth = 31;
    if (request.mins == 0) {
        if (outputChain) outputChain[0] = 0x2f0;
        return 1;
    }

    if (init(request)) {
        printf("an error occurred\n");
        return -1;
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
    solutionLength = singleSearchInner(solutionLength);

    if (solutionLength == maxDepth) solutionLength = maxDepth;
    if (accuracy == ACCURACY_REDUCED) return solutionLength;
    long totalIter = iter;
    iter = 0;

    if (hlpSolveVerbosity >= 2) printf("starting main search\n");

    _searchAccuracy = accuracy;
    int result = singleSearchInner(solutionLength - 1);
    if (hlpSolveVerbosity >= 2) printf("total iter across searches: %'ld\n", totalIter + iter);
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

void printChain(uint16_t* chain, int length) {
    const char layerStrings[][16] = {
        "%X, %X",
        "%X, *%X",
        "*%X, %X",
        "*%X, *%X",
        "^%X, *%X",
        "%X, ^*%X"
    };
    for (int i = 0; i < length; i++) {
        uint16_t conf = chain[i];
        printf(layerStrings[conf >> 8], (conf >> 4) & 15, conf & 15);
        if (i < length - 1) printf(";  ");
    }
}

void printHlpRequest(hlp_request_t request) {
    printf("%016lx", request.mins);
}

//#pragma GCC pop_options
