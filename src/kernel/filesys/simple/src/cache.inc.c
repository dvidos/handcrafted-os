#include "../dependencies/returns.h"
#include "internal.h"
#include <string.h>

typedef struct cache_entry cache_entry;
typedef struct cache_data cache_data;

#define CACHE_SLOTS        128
#define CACHE_HASH_MASK   0x7F
#define CACHE_OP_READ        0
#define CACHE_OP_WRITE       1
#define CACHE_OP_WIPE        2

struct cache_entry {
    uint32_t block_no;
    void *data_ptr; // just pointer, not owner of memory
    uint8_t is_dirty: 1;
    cache_entry *lru_prev; // prev,next for LRU eviction, next points from head to tail
    cache_entry *lru_next;
    cache_entry *hash_next; // for collisions
};

struct cache_data {
    sector_device *device;
    mem_allocator *memory;
    uint32_t block_size;
    void *big_data_buffer;
    cache_entry entries_arr[CACHE_SLOTS];
    int next_unused_entry;

    cache_entry *hashtable[CACHE_SLOTS];
    cache_entry *lru_list_head;
    cache_entry *lru_list_tail;
};

static cache_data *initialize_cache(mem_allocator *memory, sector_device *device, uint32_t block_size) {
    cache_data *data = memory->allocate(memory, sizeof(cache_data));

    memset(data, 0, sizeof(cache_data));
    data->memory = memory;
    data->device = device;
    data->block_size = block_size;
    data->big_data_buffer = memory->allocate(memory, block_size * CACHE_SLOTS);

    for (int i = 0; i < CACHE_SLOTS; i++)
        data->entries_arr[i].data_ptr = data->big_data_buffer + (i * block_size);

    return data;
}

static inline int cache_load_block_raw(cache_data *data, uint32_t block_no, void *buffer) {
    uint32_t sector_size = data->device->get_sector_size(data->device);
    int remain_size = data->block_size;
    uint32_t sector_no = block_no * sector_size;
    while (remain_size > 0) {
        int err = data->device->read_sector(data->device, sector_no, buffer);
        if (err != OK) return err;

        remain_size -= sector_size;
        buffer += sector_size;
        sector_no += 1;
    }
    return OK;
}

static inline int cache_save_block_raw(cache_data *data, uint32_t block_no, void *buffer) {
    uint32_t sector_size = data->device->get_sector_size(data->device);
    int remain_size = data->block_size;
    uint32_t sector_no = block_no * sector_size;
    while (remain_size > 0) {
        int err = data->device->read_sector(data->device, sector_no, buffer);
        if (err != OK) return err;

        remain_size -= sector_size;
        buffer += sector_size;
        sector_no += 1;
    }
    return OK;
}

static inline int cache_hashkey(uint32_t block_no) {
    return (int)(block_no & CACHE_HASH_MASK);
}

static inline void cache_promote_entry(cache_data *data, cache_entry *entry) {
    if (entry == data->lru_list_head) // already at head
        return;
    
    // remove from current position
    if (entry == data->lru_list_tail)
        data->lru_list_tail = entry->lru_prev; // "next" points towards tail
    if (entry->lru_next != NULL)
        entry->lru_next->lru_prev = entry->lru_prev;
    if (entry->lru_prev != NULL)
        entry->lru_prev->lru_next = entry->lru_next;
    
    // insert into head
    entry->lru_prev = NULL;
    entry->lru_next = data->lru_list_head;
    data->lru_list_head = entry;
}

static inline int cache_evict_least_recently_used(cache_data *data, cache_entry **evicted_ptr) {
    cache_entry *victim = data->lru_list_tail;
    if (victim == NULL) // there are no nodes in cache
        return OK;

    if (victim->is_dirty) {
        // save it first, return if error
        int err = cache_save_block_raw(data, victim->block_no, victim->data_ptr);
        if (err != OK) return err;
        victim->is_dirty = 0;
    }

    // remove from LRU list
    if (data->lru_list_head == victim) { // there is only one
        data->lru_list_head = NULL;
        data->lru_list_tail = NULL;

    } else { // there are two or more
        cache_entry *second = victim->lru_next;
        second->lru_prev = NULL;
        data->lru_list_tail = second;
    }

    // now, to remove from the (small) hash chain
    int hash_index = victim->block_no & CACHE_HASH_MASK;
    if (data->hashtable[hash_index] == victim) {
        data->hashtable[hash_index] = data->hashtable[hash_index]->hash_next;
    } else {
        cache_entry *prev = data->hashtable[hash_index];
        while (prev != NULL) {
            if (prev->hash_next == victim) {
                prev->hash_next = victim->hash_next;
                break;
            }
            prev = prev->hash_next;
        }
    }

    // update counters
    data->next_unused_entry -= 1;
    *evicted_ptr = victim;
    return OK;
}

static inline cache_entry *cache_find_entry(cache_data *data, uint32_t block_no) {
    int index = cache_hashkey(block_no);
    cache_entry *entry = data->hashtable[index];
    while (1) {
        if (entry == NULL || entry->block_no == block_no)
            return entry;
        entry = entry->hash_next;
    }
}

static inline int cache_add_entry_to_lists(cache_data *data, cache_entry *entry) {
    // add to head of the LRU list
    if (data->lru_list_head == NULL) { // there are no nodes so far
        data->lru_list_head = entry;
        data->lru_list_tail = entry;
    } else {
        entry->lru_prev = NULL;
        entry->lru_next = data->lru_list_head;
        data->lru_list_head = entry;
    }

    // add to the hashtable list head
    int index = cache_hashkey(entry->block_no);
    entry->hash_next = data->hashtable[index];
    data->hashtable[index] = entry;

    data->next_unused_entry += 1;
    return OK;
}

static int cache_find_unused_slot(cache_data *data, cache_entry **entry) {
    if (data->next_unused_entry >= CACHE_SLOTS)
        return ERR_RESOURCES_EXHAUSTED;

    *entry = &data->entries_arr[data->next_unused_entry];
    data->next_unused_entry += 1;
    return OK;
}

static int cached_io_operation(cache_data *data, int operation, uint32_t block_no, uint32_t block_offset, void *buffer, int length) {
    int err;

    cache_entry *entry = cache_find_entry(data, block_no);
    if (entry != NULL) {
        cache_promote_entry(data, entry);
    } else {
        if (data->next_unused_entry >= CACHE_SLOTS) {
            err = cache_evict_least_recently_used(data, &entry);
            if (err != OK) return err;
        } else {
            err = cache_find_unused_slot(data, &entry);
            if (err != OK) return err;
        }
        
        err = cache_load_block_raw(data, block_no, entry->data_ptr);
        if (err != OK) return err;

        cache_add_entry_to_lists(data, entry);
    }

    // stay within bounds
    if (block_offset + length > data->block_size)
        return ERR_OUT_OF_BOUNDS;

    // safe to operate
    if (operation == CACHE_OP_READ) {
        memcpy(buffer, entry->data_ptr + block_offset, length);

    } else if (operation == CACHE_OP_WRITE) {
        memcpy(entry->data_ptr + block_offset, buffer, length);
        entry->is_dirty = 1;

    } else if (operation == CACHE_OP_WIPE) {
        memset(entry->data_ptr, 0, data->block_size);
        entry->is_dirty = 1;

    } else {
        return ERR_NOT_SUPPORTED;
    }

    return OK;
}

static inline int cached_read(cache_data *data, uint32_t block_no, uint32_t block_offset, void *buffer, int length) {
    return cached_io_operation(data, CACHE_OP_READ, block_no, block_offset, buffer, length);
}

static inline int cached_write(cache_data *data, uint32_t block_no, uint32_t block_offset, void *buffer, int length) {
    return cached_io_operation(data, CACHE_OP_WRITE, block_no, block_offset, buffer, length);
}

static inline int cached_wipe(cache_data *data, uint32_t block_no) {
    return cached_io_operation(data, CACHE_OP_WIPE, block_no, 0, NULL, data->block_size);
}

static inline int cache_flush(cache_data *data) {
    int err;

    for (int i = 0; i < CACHE_SLOTS; i++) {
        cache_entry *entry = &data->entries_arr[i];
        if (!entry->is_dirty)
            continue;

        err = cache_save_block_raw(data, entry->block_no, entry->data_ptr);
        if (err != OK) return err;
        entry->is_dirty = 0;
    }

    return OK;
}

static inline void cache_release_memory(cache_data *data) {
    data->memory->release(data->memory, data->big_data_buffer);
    data->memory->release(data->memory, data);
}


