#pragma once
#include "../include/ctypes.h"


typedef struct cache cache_t;
typedef struct embedded_cache_node embedded_cache_node_t;

struct cache {
    embedded_cache_node_t *lru_newest;
    embedded_cache_node_t *lru_oldest;
    embedded_cache_node_t **hashtable_arr;

    int hashtable_entries;
    int capacity;
    int count;
    int node_struct_offset; // from host structure to embedded_cache_node member
};

// to be enbedded in the host struct
struct embedded_cache_node {
    uint64_t key;
    embedded_cache_node_t *lru_older;
    embedded_cache_node_t *lru_newer;
    embedded_cache_node_t *hashtable_next;
};


void cache_init(cache_t *cache, int capacity, int item_node_offset);

void *cache_get(cache_t *cache, uint64_t key);   // gets item, promotes to recently used
void *cache_put(cache_t *cache, uint64_t key, void *new_item); // returns evicted item, if any
void *cache_delete(cache_t *cache, uint64_t key); // // returns deleted item, if found

static inline bool cache_is_empty(cache_t *cache) { return cache->count == 0; }
static inline bool cache_is_full(cache_t *cache) { return cache->count >= cache->capacity; }
