#include "backed_cache.h"
#include "../memory/kheap.h"
#include "../utils/assert.h"
#include "../utils/mutex.h"
#include "string.h"


#define in_range(value, low, hi)     ((value) < (low) ? (low) : ((value) > (hi) ? (hi) : (value)))
#define at_most(value, hi)           ((value) > (hi) ? (hi) : (value))



static inline uint32_t next_pow2(uint32_t x) {
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

static int murmur_hash3(uint64_t key, int hash_slots) {
    // good mixing of bits, even for low entropy
    uint64_t k = key;
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k % hash_slots;
}

static backed_cache_node *find_node_in_hashtable(backed_cache_t *cache, int hash_index, uint64_t key) {
    backed_cache_node *node = cache->hashtable_ptrs[hash_index];
    while (node != NULL) {
        if (node->key == key) return node;
        node = node->hash_next;
    }
    return NULL;
}

static void promote_node_to_newest(backed_cache_t *cache, backed_cache_node *node) {
    if (node == NULL || node == cache->lru_newest)
        return;

    // remove from current position
    if (node->lru_older) node->lru_older->lru_newer = node->lru_newer;
    else                 cache->lru_oldest          = node->lru_newer;
    if (node->lru_newer) node->lru_newer->lru_older = node->lru_older;

    // insert at newest
    node->lru_older = cache->lru_newest;
    node->lru_newer = NULL;
    if (cache->lru_newest) cache->lru_newest->lru_newer = node;
    else                   cache->lru_oldest            = node;
    cache->lru_newest = node;
}

static backed_cache_node *find_an_unused_node(backed_cache_t *cache) {
    for (backed_cache_node *n = cache->lru_oldest; n != NULL; n = n->lru_newer) {
        if (!n->is_used)
            return n;
    }
    return NULL;
}

static backed_cache_node *find_an_old_unreferenced_node(backed_cache_t *cache) {
    for (backed_cache_node *n = cache->lru_oldest; n != NULL; n = n->lru_newer) {
        if (n->ref_count == 0)
            return n;
    }
    return NULL;
}

static void add_node_to_hashtable(backed_cache_t *cache, int hash_index, backed_cache_node *node) {
    // insert at head, no matter if slot is currently NULL or not
    node->hash_next = cache->hashtable_ptrs[hash_index];
    cache->hashtable_ptrs[hash_index] = node;
}

static void remove_node_from_hashtable(backed_cache_t *cache, int hash_index, backed_cache_node *node) {
    if (cache->hashtable_ptrs[hash_index] == node) {
        cache->hashtable_ptrs[hash_index] = node->hash_next;
    } else {
        for (backed_cache_node *prev = cache->hashtable_ptrs[hash_index]; prev->hash_next != NULL; prev = prev->hash_next) {
            if (prev->hash_next == node) {
                prev->hash_next = node->hash_next;
                break;
            }
        }
    }
    node->hash_next = NULL;
}

static error_t save_node(backed_cache_t *cache, backed_cache_node *node) {
    if (cache->backend.write == NULL)
        return OK;
    
    if (node->is_dirty) {
        error_t err = cache->backend.write(node->key, node->data_ptr, cache->backend.context);
        if (err) return err;

        node->is_dirty = false;
        cache->dirty_count--; // given that it was indeed dirty
    }
    return OK;
}

static error_t load_key_into_node(backed_cache_t *cache, uint64_t key, backed_cache_node *node) {
    node->key = key;
    if (cache->backend.load == NULL) {
        memset(node->data_ptr, 0, cache->obj_size);
    } else {
        error_t err = cache->backend.load(key, node->data_ptr, cache->backend.context);
        if (err) return err;
    }
    if (!node->is_used) {
        node->is_used = true;
        cache->used_count++;
    }
    return OK;
}

static void make_node_unused_again(backed_cache_t *cache, backed_cache_node *node) {

    // caller MUST ensure that node is not referenced at all
    ASSERT(node->ref_count == 0);

    if (node->is_used) {
        node->is_used = false;
        cache->used_count--;
    }
    if (node->is_dirty) {
        node->is_dirty = false;
        cache->dirty_count--;
    }

    remove_node_from_hashtable(cache, murmur_hash3(node->key, cache->hashtable_capacity), node);
}

static error_t ensure_node_in_cache(backed_cache_t *cache, uint64_t key, backed_cache_node **out_ptr) {
    error_t err;
    int hash_index = murmur_hash3(key, cache->hashtable_capacity);
    
    backed_cache_node *node = find_node_in_hashtable(cache, hash_index, key);  // is it already here?
    if (node != NULL) { 
        promote_node_to_newest(cache, node);
        *out_ptr = node;
        return OK;
    }

    // we'll need to load. find empty slot or evict
    if (cache->used_count < cache->obj_capacity) {
        node = find_an_unused_node(cache);
        if (node == NULL) return ERR_CORRUPTION_DETECTED; // used_count is out of sync
        node->is_used = 1;
        cache->used_count++;

    } else {
        node = find_an_old_unreferenced_node(cache);
        if (node == NULL) return ERR_CONTAINER_FULL; // all are in use, we need more space
        err = save_node(cache, node);
        if (err) return err;
        make_node_unused_again(cache, node);
    }

    load_key_into_node(cache, key, node);
    add_node_to_hashtable(cache, hash_index, node);
    promote_node_to_newest(cache, node);
    *out_ptr = node;
    return OK;
}

// --------------------------------------------------------------------------

static error_t backed_cache_acquire(backed_cache_t *cache, uint64_t key, void **out_ptr) {
    mutex_acquire(&cache->lock);

    backed_cache_node *node;
    error_t err = ensure_node_in_cache(cache, key, &node);
    if (err) { mutex_release(&cache->lock); return ERR_NOT_FOUND; }

    node->ref_count++;
    cache->references_sum++;
    *out_ptr = node->data_ptr;
    mutex_release(&cache->lock);
    return OK;
}

static error_t backed_cache_mark_dirty(backed_cache_t *cache, uint64_t key) {
    mutex_acquire(&cache->lock);
    
    int hash_index = murmur_hash3(key, cache->hashtable_capacity);
    backed_cache_node *node = find_node_in_hashtable(cache, hash_index, key);
    if (node == NULL) { mutex_release(&cache->lock); return ERR_NOT_FOUND; }

    promote_node_to_newest(cache, node);
    if (!node->is_dirty) {
        node->is_dirty = true;
        cache->dirty_count++;
    }

    mutex_release(&cache->lock);
    return OK;
}

static error_t backed_cache_release(backed_cache_t *cache, uint64_t key) {
    mutex_acquire(&cache->lock);

    int hash_index = murmur_hash3(key, cache->hashtable_capacity);
    backed_cache_node *node = find_node_in_hashtable(cache, hash_index, key);
    if (node == NULL) { mutex_release(&cache->lock); return ERR_NOT_FOUND; }
    
    promote_node_to_newest(cache, node);
    if (node->ref_count == 0) { mutex_release(&cache->lock); return ERR_UNDERFLOW; }
    
    node->ref_count--;
    cache->references_sum--;

    mutex_release(&cache->lock);
    return OK;
}

static error_t backed_cache_read(backed_cache_t *cache, uint64_t key, void *buffer) {
    mutex_acquire(&cache->lock);

    backed_cache_node *node;
    error_t err = ensure_node_in_cache(cache, key, &node);
    if (err) { mutex_release(&cache->lock); return err; }

    memcpy(buffer, node->data_ptr, cache->obj_size);
    mutex_release(&cache->lock);
    return OK;
}

static error_t backed_cache_write(backed_cache_t *cache, uint64_t key, void *buffer) {
    mutex_acquire(&cache->lock);

    backed_cache_node *node;
    error_t err = ensure_node_in_cache(cache, key, &node);
    if (err) { mutex_release(&cache->lock); return err; }

    memcpy(node->data_ptr, buffer, cache->obj_size);
    if (!node->is_dirty) {
        node->is_dirty = true;
        cache->dirty_count++;
    }
    mutex_release(&cache->lock);
    return OK;
}

static error_t backed_cache_fill(backed_cache_t *cache, uint64_t key, char value) {
    mutex_acquire(&cache->lock);

    backed_cache_node *node;
    error_t err = ensure_node_in_cache(cache, key, &node);
    if (err) { mutex_release(&cache->lock); return err; }

    memset(node->data_ptr, value, cache->obj_size);
    if (!node->is_dirty) {
        node->is_dirty = true;
        cache->dirty_count++;
    }
    mutex_release(&cache->lock);
    return OK;
}

static error_t backed_cache_read_part(backed_cache_t *cache, uint64_t key, size_t offset, void *part_buffer, size_t part_len) {
    mutex_acquire(&cache->lock);

    backed_cache_node *node;
    error_t err = ensure_node_in_cache(cache, key, &node);
    if (err) { mutex_release(&cache->lock); return err; }

    offset = at_most(offset, cache->obj_size);
    part_len = at_most(part_len, cache->obj_size - offset);
    memcpy(part_buffer, node->data_ptr + offset, part_len);
    mutex_release(&cache->lock);
    return OK;
}

static error_t backed_cache_write_part(backed_cache_t *cache, uint64_t key, size_t offset, void *part_buffer, size_t part_len) {
    mutex_acquire(&cache->lock);

    backed_cache_node *node;
    error_t err = ensure_node_in_cache(cache, key, &node);
    if (err) { mutex_release(&cache->lock); return err; }

    offset = at_most(offset, cache->obj_size);
    part_len = at_most(part_len, cache->obj_size - offset);
    memcpy(node->data_ptr + offset, part_buffer, part_len);
    if (!node->is_dirty) {
        node->is_dirty = true;
        cache->dirty_count++;
    }
    mutex_release(&cache->lock);
    return OK;
}

static error_t backed_cache_fill_part(backed_cache_t *cache, uint64_t key, size_t offset, char value, size_t part_len) {
    mutex_acquire(&cache->lock);

    backed_cache_node *node;
    error_t err = ensure_node_in_cache(cache, key, &node);
    if (err) { mutex_release(&cache->lock); return err; }

    offset = at_most(offset, cache->obj_size);
    part_len = at_most(part_len, cache->obj_size - offset);
    memset(node->data_ptr + offset, value, part_len);
    if (!node->is_dirty) {
        node->is_dirty = true;
        cache->dirty_count++;
    }
    mutex_release(&cache->lock);
    return OK;
}

static error_t backed_cache_invalidate(backed_cache_t *cache, uint64_t key) {
    mutex_acquire(&cache->lock);

    int hash_index = murmur_hash3(key, cache->hashtable_capacity);
    backed_cache_node *node = find_node_in_hashtable(cache, hash_index, key);
    
    if (node == NULL)        { mutex_release(&cache->lock); return OK; }
    if (node->ref_count > 0) { mutex_release(&cache->lock); return ERR_BUSY; }
    make_node_unused_again(cache, node);

    mutex_release(&cache->lock);
    return OK;
}

static error_t backed_cache_flush_all(backed_cache_t *cache) {
    if (cache->backend.write == NULL)
        return ERR_NOT_SUPPORTED;
    
    mutex_acquire(&cache->lock);
    for (backed_cache_node *n = cache->lru_oldest; n; n = n->lru_newer) {
        save_node(cache, n); // ignore errors
    }
    mutex_release(&cache->lock);

    return OK;
}

static error_t backed_cache_destroy(backed_cache_t *cache) {
    ASSERT(cache != NULL);
    if (cache->objs_arr)       kfree(cache->objs_arr);
    if (cache->nodes_arr)      kfree(cache->nodes_arr);
    if (cache->hashtable_ptrs) kfree(cache->hashtable_ptrs);
    return OK;
}

// -----------------------------------------------------------


static backed_cache_ops ops = {
    .acquire    = backed_cache_acquire,
    .mark_dirty = backed_cache_mark_dirty,
    .release    = backed_cache_release,
    .read       = backed_cache_read,
    .write      = backed_cache_write,
    .fill       = backed_cache_fill,
    .read_part  = backed_cache_read_part,
    .write_part = backed_cache_write_part,
    .fill_part  = backed_cache_fill_part,
    .invalidate = backed_cache_invalidate,
    .flush_all  = backed_cache_flush_all,
    .destroy    = backed_cache_destroy,
};

void initialize_backed_cache(
    backed_cache_t *cache,
    backed_cache_backend backend,
    size_t obj_size,
    size_t obj_capacity
) {
    memset(cache, 0, sizeof(backed_cache_t));

    cache->backend = backend;
    cache->obj_capacity = obj_capacity;
    cache->obj_size = obj_size;
    cache->hashtable_capacity = next_pow2(obj_capacity * 2);

    cache->objs_arr = kmalloc(obj_capacity * obj_size);
    cache->nodes_arr = kmalloc(obj_capacity * sizeof(backed_cache_node));
    cache->hashtable_ptrs = kmalloc(cache->hashtable_capacity * sizeof(backed_cache_node *));

    memset(cache->objs_arr, 0, obj_capacity * obj_size);
    memset(cache->nodes_arr, 0, obj_capacity * sizeof(backed_cache_node));
    memset(cache->hashtable_ptrs, 0, cache->hashtable_capacity * sizeof(backed_cache_node *));

    cache->lru_oldest = &cache->nodes_arr[0];
    cache->lru_newest = &cache->nodes_arr[cache->obj_capacity - 1];

    for (size_t i = 0; i < obj_capacity; i++)
        cache->nodes_arr[i].data_ptr = cache->objs_arr + (i * cache->obj_size);

    for (size_t i = 1; i < obj_capacity; i++)
        cache->nodes_arr[i].lru_older = &cache->nodes_arr[i - 1];

    for (size_t i = 0; i < obj_capacity - 1; i++)
        cache->nodes_arr[i].lru_newer = &cache->nodes_arr[i + 1];

    cache->ops = &ops;
}

