#ifndef CACHE_H
#define CACHE_H
#include <immintrin.h>
#include <stdint.h>

struct cache_entry {
    uint64_t value;
    uint32_t trial;
    uint8_t depth;
};

struct cache {
    struct cache_entry* array;
    uint64_t mask;
    uint32_t global_trial;
    int size_log;
    struct cache_stats {
        long total_checks, same_depth_hits, dif_layer_hits, misses, bucket_util;
    } stats;
};

static struct cache main_cache = {0};

static int cache_check(struct cache* cache, uint64_t value, int depth) {
    uint32_t pos = _mm_crc32_u32(_mm_crc32_u32(0, value & UINT32_MAX), value >> 32) & cache->mask;
    struct cache_entry* entry = cache->array + pos;
    cache->stats.total_checks++;
    if (entry->value == value && entry->depth <= depth && entry->trial == cache->global_trial) {
        if (entry->depth == depth) cache->stats.same_depth_hits++;
        else cache->stats.dif_layer_hits++;
        return 1;
    }

    if (entry->trial == cache->global_trial && entry->value != value) cache->stats.misses++;
    else cache->stats.bucket_util++;

    entry->value = value;
    entry->depth = depth;
    entry->trial = cache->global_trial;

    return 0;
}

static void invalidate_cache(struct cache* cache) {
    cache->global_trial++;
    // clear the cache if we somehow hit overflow
    if (!cache->global_trial) {
        for (int i = 0; i <= cache->mask; i++) {
            cache->array[i].value = 0;
            cache->array[i].depth = 0;
            cache->array[i].trial = 0;
        }
        // trial 0 should always mean blank
        cache->global_trial++;
    }
}

static void cache_init(struct cache* cache) {
    if (cache->array) return;
    cache->array = calloc((1 << cache->size_log), sizeof(struct cache_entry));
    cache->global_trial = 0;
    cache->mask = (1 << cache->size_log) - 1;
}

static void cache_free(struct cache* cache) {
    if (!cache->array) return;
    free(cache->array);
}

static void cache_print_stats(struct cache* cache) {
    printf("cache checks: %'ld; same depth hits: %'ld; dif layer hits: %'ld; misses: %'ld; bucket utilization: %'ld\n",
            cache->stats.total_checks,
            cache->stats.same_depth_hits,
            cache->stats.dif_layer_hits,
            cache->stats.misses,
            cache->stats.bucket_util);
}

#endif
