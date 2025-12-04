#include "../dependencies/returns.h"
#include "internal.h"
#include <string.h>

typedef struct bcache_entry bcache_entry;
typedef struct block_cache block_cache;

#define CACHE_OP_READ        0
#define CACHE_OP_WRITE       1
#define CACHE_OP_WIPE        2

struct bcache_entry {
    uint32_t block_no;
    void *data_ptr; // just pointer, not owner of memory
    uint8_t is_used: 1;
    uint8_t is_dirty: 1;
    bcache_entry *lru_older; // prev,next for LRU eviction, next points from head to tail
    bcache_entry *lru_newer; // next points to tail
    bcache_entry *hash_next; // for collisions
};

struct block_cache {
    sector_device *device;
    uint32_t block_size;
    void *big_data_buffer;
    bcache_entry entries_arr[CACHE_SLOTS];
    int entries_used;

    bcache_entry *hashtable[CACHE_SLOTS];
    bcache_entry *lru_list_newest;
    bcache_entry *lru_list_oldest;
};

static block_cache *initialize_block_cache(mem_allocator *mem, sector_device *device, uint32_t block_size) {
    block_cache *data = mem->allocate(mem, sizeof(block_cache));

    memset(data, 0, sizeof(block_cache));
    data->device = device;
    data->block_size = block_size;
    data->big_data_buffer = mem->allocate(mem, block_size * CACHE_SLOTS);

    for (int i = 0; i < CACHE_SLOTS; i++)
        data->entries_arr[i].data_ptr = data->big_data_buffer + (i * block_size);

    return data;
}

static inline int bcache_load_block_raw(block_cache *data, uint32_t block_no, void *buffer) {
    uint32_t sector_size = data->device->get_sector_size(data->device);
    int remain_size = data->block_size;
    uint32_t sector_no = block_no * (data->block_size / sector_size);
    while (remain_size > 0) {
        int err = data->device->read_sector(data->device, sector_no, buffer);
        if (err != OK) return err;

        remain_size -= sector_size;
        buffer += sector_size;
        sector_no += 1;
    }
    return OK;
}

static inline int bcache_save_block_raw(block_cache *data, uint32_t block_no, void *buffer) {
    uint32_t sector_size = data->device->get_sector_size(data->device);
    int remain_size = data->block_size;
    uint32_t sector_no = block_no * (data->block_size / sector_size);
    while (remain_size > 0) {
        int err = data->device->write_sector(data->device, sector_no, buffer);
        if (err != OK) return err;

        remain_size -= sector_size;
        buffer += sector_size;
        sector_no += 1;
    }
    return OK;
}

static inline int bcache_hashkey(uint32_t block_no) {
    return (int)(block_no % CACHE_SLOTS);
}

static inline void bcache_promote_recently_used_entry(block_cache *data, bcache_entry *entry) {
    if (entry == data->lru_list_newest)
        return;
    
    // remove from current position
    if (entry == data->lru_list_oldest)
        data->lru_list_oldest = entry->lru_newer;
    if (entry->lru_newer != NULL)
        entry->lru_newer->lru_older = entry->lru_older;
    if (entry->lru_older != NULL)
        entry->lru_older->lru_newer = entry->lru_newer;
    
    // insert into newest side
    if (data->lru_list_newest != NULL)
        data->lru_list_newest->lru_newer = entry;
    entry->lru_older = data->lru_list_newest;
    entry->lru_newer = NULL;
    data->lru_list_newest = entry;
}

static inline int bcache_evict_least_recently_used(block_cache *data, bcache_entry **evicted_ptr) {
    bcache_entry *oldest = data->lru_list_oldest;
    if (oldest == NULL) // there are no nodes in cache
        return OK;

    if (oldest->is_dirty) {
        // save it first, return if error
        int err = bcache_save_block_raw(data, oldest->block_no, oldest->data_ptr);
        if (err != OK) return err;
        oldest->is_dirty = 0;
    }

    // remove from LRU list
    if (data->lru_list_newest == oldest) {
        // there is only one node
        data->lru_list_newest = NULL;
        data->lru_list_oldest = NULL;

    } else {
        // there are two or more nodes
        bcache_entry *second_oldest = oldest->lru_newer;
        second_oldest->lru_older = NULL;
        data->lru_list_oldest = second_oldest;
    }

    // now, to remove from the (small) hash chain
    int hash_index = bcache_hashkey(oldest->block_no);
    if (data->hashtable[hash_index] == oldest) {
        data->hashtable[hash_index] = data->hashtable[hash_index]->hash_next;
    } else {
        bcache_entry *prev = data->hashtable[hash_index];
        while (prev != NULL) {
            if (prev->hash_next == oldest) {
                prev->hash_next = oldest->hash_next;
                break;
            }
            prev = prev->hash_next;
        }
    }

    data->entries_used -= 1;
    oldest->is_used = 0;
    *evicted_ptr = oldest;
    return OK;
}

static inline bcache_entry *bcache_find_entry_from_hashtable(block_cache *data, uint32_t block_no) {
    int index = bcache_hashkey(block_no);
    bcache_entry *entry = data->hashtable[index];
    if (entry == NULL)
        return NULL; // not found

    while (entry != NULL) {
        if (entry->block_no == block_no)
            return entry; // found
        entry = entry->hash_next;
    }

    return NULL; // not found
}

static inline int bcache_add_entry_to_lists(block_cache *data, bcache_entry *entry) {
    // add to head of the LRU list
    if (data->lru_list_newest == NULL) {
        // there are no nodes so far
        data->lru_list_newest = entry;
        data->lru_list_oldest = entry;
        entry->lru_newer = NULL;
        entry->lru_older = NULL;
    } else {
        // there is at least one entry in the list, insert at head
        data->lru_list_newest->lru_newer = entry;
        entry->lru_older = data->lru_list_newest;
        entry->lru_newer = NULL;
        data->lru_list_newest = entry;
    }

    // add to the hashtable list head
    int index = bcache_hashkey(entry->block_no);
    entry->hash_next = data->hashtable[index];
    data->hashtable[index] = entry;

    return OK;
}

static int bcache_find_unused_slot(block_cache *data, bcache_entry **entry) {
    if (data->entries_used >= CACHE_SLOTS)
        return ERR_RESOURCES_EXHAUSTED;
    
    // maybe later we use a bitmap for faster lookups
    for (int i = 0; i < CACHE_SLOTS; i++) {
        if (!data->entries_arr[i].is_used) {
            *entry = &data->entries_arr[i];
            return OK;
        }
    }

    return ERR_RESOURCES_EXHAUSTED;
}

static int bcached_io_operation(block_cache *data, int operation, uint32_t block_no, uint32_t block_offset, void *buffer, int length) {
    int err;

    bcache_entry *entry = bcache_find_entry_from_hashtable(data, block_no);
    if (entry != NULL) {
        bcache_promote_recently_used_entry(data, entry);
    } else {
        if (data->entries_used >= CACHE_SLOTS) {
            err = bcache_evict_least_recently_used(data, &entry);
            if (err != OK) return err;
        } else {
            err = bcache_find_unused_slot(data, &entry);
            if (err != OK) return err;
        }

        entry->is_used = 1;
        data->entries_used++;
        
        err = bcache_load_block_raw(data, block_no, entry->data_ptr);
        if (err != OK) return err;

        entry->block_no = block_no;
        entry->is_dirty = 0;

        bcache_add_entry_to_lists(data, entry);
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

static inline int bcache_read(block_cache *data, uint32_t block_no, uint32_t block_offset, void *buffer, int length) {
    return bcached_io_operation(data, CACHE_OP_READ, block_no, block_offset, buffer, length);
}

static inline int bcache_write(block_cache *data, uint32_t block_no, uint32_t block_offset, void *buffer, int length) {
    return bcached_io_operation(data, CACHE_OP_WRITE, block_no, block_offset, buffer, length);
}

static inline int bcache_wipe(block_cache *data, uint32_t block_no) {
    return bcached_io_operation(data, CACHE_OP_WIPE, block_no, 0, NULL, data->block_size);
}

static inline int bcache_flush(block_cache *data) {
    int err;

    for (int i = 0; i < CACHE_SLOTS; i++) {
        bcache_entry *entry = &data->entries_arr[i];
        if (!entry->is_used || !entry->is_dirty)
            continue;

        err = bcache_save_block_raw(data, entry->block_no, entry->data_ptr);
        if (err != OK) return err;
        entry->is_dirty = 0;
    }

    return OK;
}

static inline void bcache_release_memory(block_cache *data, mem_allocator *mem) {
    mem->release(mem, data->big_data_buffer);
    mem->release(mem, data);
}

static void bcache_dump_debug_info(block_cache *data) {
    bcache_entry *e;

    int entries_used = 0;
    int entries_dirty = 0;
    for (int i = 0; i < CACHE_SLOTS; i++) {
        if (data->entries_arr[i].is_used)
            entries_used++;
        if (data->entries_arr[i].is_dirty)
            entries_dirty++;
    }
    int hash_used_slots = 0;
    for (int i = 0; i < CACHE_SLOTS; i++)
        if (data->hashtable[i] != NULL)
            hash_used_slots++;

    printf("Cache info (BlockSize:%d EntriesUsed:%d)\n", data->block_size, data->entries_used);
    printf("    Entries array (%d total, %d used, %d dirty)\n", CACHE_SLOTS, entries_used, entries_dirty);
    // for (int i = 0; i < CACHE_SLOTS; i++) {
    //     e = &data->entries_arr[i];
    //     if (!e->is_used) continue;
    //     printf("        [%d] [block:%d%s]\n", i, e->block_no, e->is_dirty ? "*" : "");
    // }

    printf("    Hashtable (%d slots, %d used)\n", CACHE_SLOTS, hash_used_slots);
    for (int i = 0; i < CACHE_SLOTS; i++) {
        e = data->hashtable[i];
        if (e == NULL) continue;

        printf("        [%d]", i);
        while (e != NULL) {
            printf("-->[block:%d%s]", e->block_no, e->is_dirty ? "*" : "");
            e = e->hash_next;
        }
        printf("-->null\n");
    }

    printf("    Recently Used List\n");
    printf("        Newest");
    e = data->lru_list_newest;
    while (e != NULL) {
        printf("-->[block:%d%s]", e->block_no, e->is_dirty ? "*" : "");
        e = e->lru_older;
    }
    printf("-->null\n");
    printf("        Oldest");
    e = data->lru_list_oldest;
    while (e != NULL) {
        printf("-->[block:%d%s]", e->block_no, e->is_dirty ? "*" : "");
        e = e->lru_newer;
    }
    printf("-->null\n");
}

