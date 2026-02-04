#include "cache.h"
#include "../memory/kheap.h"
#include "../klib/string.h"


static inline uint32_t next_pow2(uint32_t x) {
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

void cache_init(cache_t *cache, int capacity, int item_node_offset) {
    memset(cache, 0, sizeof(cache_t));

    // hashtable size must be the next power of two, from capacity
    cache->hashtable_entries = next_pow2(capacity * 2);
    int hashtable_size = cache->hashtable_entries * sizeof(embedded_cache_node_t *);
    cache->hashtable_arr = (embedded_cache_node_t **)kmalloc(hashtable_size);
    memset(cache->hashtable_arr, 0, hashtable_size);

    cache->capacity = capacity;
    cache->node_struct_offset = item_node_offset;
}

void cache_destroy(cache_t *cache) {
    if (cache && cache->hashtable_arr)
        kfree(cache->hashtable_arr);
    memset(cache, 0, sizeof(cache_t));
}

static inline uint32_t cache_hash_index(cache_t *c, uint64_t key) {
    return key & (c->hashtable_entries - 1);
}

static inline void *cache_node_ptr_to_item_ptr(cache_t *cache, embedded_cache_node_t *node) {
    return node == NULL ? NULL : ((void *)node) - cache->node_struct_offset;
}

static inline embedded_cache_node_t *cache_item_ptr_to_node_ptr(cache_t *cache, void *item) {
    return item == NULL ? NULL : (embedded_cache_node_t *)(item + cache->node_struct_offset);
}

static inline void cache_promote(cache_t *cache, void *item) {
    embedded_cache_node_t *n = cache_item_ptr_to_node_ptr(cache, item);
    if (n == NULL || n == cache->lru_newest)
        return;

    /* unlink from current position */
    if (n->lru_older)
        n->lru_older->lru_newer = n->lru_newer;
    else
        cache->lru_oldest = n->lru_newer;

    if (n->lru_newer)
        n->lru_newer->lru_older = n->lru_older;

    /* insert at newest */
    n->lru_older = cache->lru_newest;
    n->lru_newer = NULL;

    if (cache->lru_newest)
        cache->lru_newest->lru_newer = n;
    else
        cache->lru_oldest = n;

    cache->lru_newest = n;
}

void *cache_get(cache_t *cache, uint64_t key) {
    uint32_t idx = cache_hash_index(cache, key);
    embedded_cache_node_t *n = cache->hashtable_arr[idx];

    while (n) {
        if (n->key == key) {
            void *item = cache_node_ptr_to_item_ptr(cache, n);
            cache_promote(cache, item);
            return item;
        }
        n = n->hashtable_next;
    }
    return NULL;
}

// returns deleted item, if found
void *cache_delete(cache_t *cache, uint64_t key) {
    uint32_t idx = cache_hash_index(cache, key);
    embedded_cache_node_t **prev_ptr_address = &cache->hashtable_arr[idx];
    embedded_cache_node_t *n = *prev_ptr_address;

    while (n) {
        if (n->key == key) {
            void *deleted_item = cache_node_ptr_to_item_ptr(cache, n);

            // unlink from hashtable
            *prev_ptr_address = n->hashtable_next;

            // unlink from LRU */
            if (n->lru_older) n->lru_older->lru_newer = n->lru_newer;
            else              cache->lru_oldest       = n->lru_newer;

            if (n->lru_newer) n->lru_newer->lru_older = n->lru_older;
            else              cache->lru_newest       = n->lru_older;

            cache->count--;
            return deleted_item;
        }

        prev_ptr_address = &n->hashtable_next;
        n = n->hashtable_next;
    }

    return NULL;
}

// returns evicted item, if any
void *cache_put(cache_t *cache, uint64_t key, void *new_item) {
    if (!cache || !new_item) return NULL;

    uint32_t idx = cache_hash_index(cache, key);
    embedded_cache_node_t *n = cache->hashtable_arr[idx];
    void *evicted_item = NULL;

    // Check if the key already exists
    while (n) {
        if (n->key == key) {
            // Key exists → just promote
            cache_promote(cache, new_item);
            return NULL;
        }
        n = n->hashtable_next;
    }

    // Evict LRU if at capacity
    if (cache->count >= cache->capacity && cache->lru_oldest) {
        evicted_item = cache_node_ptr_to_item_ptr(cache, cache->lru_oldest);
        cache_delete(cache, cache->lru_oldest->key);
    }

    // Initialize node
    embedded_cache_node_t *node = cache_item_ptr_to_node_ptr(cache, new_item);
    node->key = key;

    // Insert into hashtable (at head of chain)
    node->hashtable_next = cache->hashtable_arr[idx];
    cache->hashtable_arr[idx] = node;

    // Promote to newest in LRU
    cache_promote(cache, new_item);

    cache->count++;
    return evicted_item;
}
