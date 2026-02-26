#pragma once
#include "../include/ctypes.h"
#include "../include/uapi/errors.h"
#include "../utils/mutex.h"


// this is a backing thing that will:
// - act as a pool of prealocated memory
// - act as a dirty tracking thing, to save on eviction or flush
// - act as a refcount tracker, to not evict while something is in use
// used for:
// - inodes cache, blocks cache, inode cache (not backed)

typedef struct backed_cache          backed_cache_t;
typedef struct backed_cache_backend  backed_cache_backend;
typedef struct backed_cache_ops      backed_cache_ops;
typedef struct backed_cache_node     backed_cache_node;
typedef struct backed_cache_data     backed_cache_data;


struct backed_cache_backend {
    error_t (*load)(uint64_t key, void *obj_data, void *context);
    error_t (*write)(uint64_t key, void *obj_data, void *context);
    void *context;
};

struct backed_cache {
    backed_cache_backend backend;

    size_t obj_capacity;
    size_t obj_size;
    uint32_t used_count;
    uint32_t dirty_count;
    size_t references_sum;

    backed_cache_node *lru_newest;
    backed_cache_node *lru_oldest;
    backed_cache_node **hashtable_ptrs;
    size_t hashtable_capacity;
    void *objs_arr;            // where we keep the data
    backed_cache_node *nodes_arr; // enough nodes for mgmt

    backed_cache_ops *ops;
    lock_t lock;
};


struct backed_cache_node {
    uint64_t key;
    backed_cache_node *lru_newer;
    backed_cache_node *lru_older;
    backed_cache_node *hash_next;
    unsigned is_used: 1;
    unsigned is_dirty: 1;
    unsigned ref_count;
    void *data_ptr; // points into the slab
};

struct backed_cache_ops {
    // operations needed by in-memory users of cache
    error_t (*get)(backed_cache_t *cache, uint64_t key, void **out_ptr);

    error_t (*lock)(backed_cache_t *cache, uint64_t key);
    error_t (*unlock)(backed_cache_t *cache, uint64_t key);
    bool    (*is_locked)(backed_cache_t *cache, uint64_t key);

    error_t (*mark_dirty)(backed_cache_t *cache, uint64_t key);
    bool    (*is_dirty)(backed_cache_t *cache, uint64_t key);
    
    // reads and writes full objects
    error_t (*read)(backed_cache_t *cache, uint64_t key, void *buffer);
    error_t (*write)(backed_cache_t *cache, uint64_t key, void *buffer);
    error_t (*fill)(backed_cache_t *cache, uint64_t key, char value);

    // reads and writes parts of objects
    error_t (*read_part)(backed_cache_t *cache, uint64_t key, size_t offset, void *part_buffer, size_t part_len);
    error_t (*write_part)(backed_cache_t *cache, uint64_t key, size_t offset, void *part_buffer, size_t part_len);
    error_t (*fill_part)(backed_cache_t *cache, uint64_t key, size_t offset, char value, size_t part_len);

    error_t (*invalidate)(backed_cache_t *cache, uint64_t key);
    error_t (*flush)(backed_cache_t *cache, uint64_t key);
    error_t (*flush_all)(backed_cache_t *cache);
    error_t (*destroy)(backed_cache_t *cache);
};

backed_cache_t *create_backed_cache(size_t object_size, size_t object_capacity, backed_cache_backend backend);
