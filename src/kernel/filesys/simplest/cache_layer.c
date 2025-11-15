#include "returns.h"
#include "internal.h"
#include <string.h>

typedef struct cache_layer_data cache_layer_data;
typedef struct cache_slot cache_slot;

struct cache_layer_data {
    mem_allocator *memory;
    sector_device *device;

    // somewhere a map of blocks loaded (hashmap array-ptr to slots)
    // somewhere a way to find LRU for eviction (double linked list)

    void *big_data_buffer; // shared buffer for all blocks
    cache_slot *slots_array;
    int slots_count;
    int used_slots_bitmap_size;
    uint8_t *used_blocks_bitmap; // array of sufficient bits

    int block_size_bytes;
    int block_size_sectors;
    int sector_size_bytes;
};

struct cache_slot {
    uint32_t block_no;
    uint8_t is_dirty;
    void *data_ptr; // just pointer, not owner of memory
};

// ------------------------------------------------------------------

static int read_slot_from_disk(cache_layer_data *d, int block_no, cache_slot *slot) {
    uint32_t first_sector = block_no * d->block_size_sectors;
    void *target = slot->data_ptr;
    for (int i = 0; i < d->block_size_sectors; i++) {
        int err = d->device->read_sector(d->device, first_sector + i, target);
        if (err != OK) return err;
        target += d->sector_size_bytes;
    }
    slot->block_no = block_no;
    slot->is_dirty = 0;
    return OK;
}

static int write_slot_to_disk(cache_layer_data *d, cache_slot *slot) {
    uint32_t first_sector = slot->block_no * d->block_size_sectors;
    void *source = slot->data_ptr;
    for (int i = 0; i < d->block_size_sectors; i++) {
        int err = d->device->write_sector(d->device, first_sector + i, source);
        if (err != OK) return err;
        source += d->sector_size_bytes;
    }
    slot->is_dirty = 0;
    return OK;
}

static cache_slot *find_slot_by_block_no(cache_layer_data *d, uint32_t block_no) {
    // stupidly linear for now
    // ideally, an array of 128 pointer, hashing by block no
    // we could also start with a hash of the block no and move on.
    for (int i = 0; i < d->slots_count; i++) {
        if (d->slots_array[i].block_no == block_no)
            return &d->slots_array[i];
    }

    return NULL;
}

static cache_slot *find_slot_to_evict(cache_layer_data *d) {
    // stupidly stupid for now
    // ideally, we shall have a double linked list, where operations move a slot to the front, we evict from the back
    static int victim = 0; 
    victim = (victim + 1) % d->slots_count;
    return &d->slots_array[victim];
}

static int find_and_ensure_block_in_cache(cache_layer_data *d, uint32_t block_no, cache_slot **slot_ptr) {
    int err;

    cache_slot *slot = find_slot_by_block_no(d, block_no);
    if (slot == NULL) {

        // we don't have it, assume we don't have empty slots
        slot = find_slot_to_evict(d);
        if (slot->is_dirty) {
            err = write_slot_to_disk(d, slot);
            if (err != OK) return err;
        }

        // load into cache
        err = read_slot_from_disk(d, block_no, slot);
        if (err != OK) return err;
    }

    *slot_ptr = slot;
    return OK;
}

// ------------------------------------------------------------------

static int cache_read(cache_layer *cl, uint32_t block_no, int offset_in_block, void *buffer, int length) {
    cache_layer_data *data = (cache_layer_data *)cl->data;
    if (offset_in_block + length > data->block_size_bytes)
        return ERR_OUT_OF_BOUNDS;

    cache_slot *slot;
    int err = find_and_ensure_block_in_cache(data, block_no, &slot);
    if (err != OK) return err;

    // now that we have it, read it
    memcpy(buffer, slot->data_ptr + offset_in_block, length);
    return OK;
}

static int cache_write(cache_layer *cl, uint32_t block_no, int offset_in_block, void *buffer, int length) {
    cache_layer_data *data = (cache_layer_data *)cl->data;
    if (offset_in_block + length > data->block_size_bytes)
        return ERR_OUT_OF_BOUNDS;

    cache_slot *slot;
    int err = find_and_ensure_block_in_cache(data, block_no, &slot);
    if (err != OK) return err;
        
    // now that we have it, write it, flag as dirty
    memcpy(slot->data_ptr + offset_in_block, buffer, length);
    slot->is_dirty = 1;
    return OK;
}

static int cache_flush(cache_layer *cl) {
    cache_layer_data *data = (cache_layer_data *)cl->data;

    for (int i = 0; i < data->slots_count; i++) {
        cache_slot *slot = &data->slots_array[i];
        if (!slot->is_dirty)
            continue;

        int err = write_slot_to_disk(data, slot);
        if (err != OK) return err;
    }

    return OK;
}

static int cache_free_memory(cache_layer *cl) {
    cache_layer_data *data = (cache_layer_data *)cl->data;

    mem_allocator *mem = data->memory;
    mem->free(mem, data->big_data_buffer);
    mem->free(mem, data->slots_array);
    mem->free(mem, data->used_blocks_bitmap);
    mem->free(mem, data);
    mem->free(mem, cl);

    return OK;
}

cache_layer *new_cache_layer(mem_allocator *memory, sector_device *device, int block_size_bytes, int blocks_to_cache) {
    cache_layer_data *data = memory->allocate(memory, sizeof(cache_layer_data));
    data->memory = memory;
    data->device = device;

    data->big_data_buffer = memory->allocate(memory, block_size_bytes * blocks_to_cache);
    data->slots_array = memory->allocate(memory, sizeof(cache_slot) * blocks_to_cache);
    data->slots_count = blocks_to_cache;
    data->block_size_bytes = block_size_bytes;
    data->block_size_sectors = block_size_bytes / device->get_sector_size(device);
    data->sector_size_bytes = device->get_sector_size(device);

    data->used_slots_bitmap_size = (blocks_to_cache + 7) / 8;
    data->used_blocks_bitmap = memory->allocate(memory, data->used_slots_bitmap_size);

    for (int i = 0; i < blocks_to_cache; i++) {
        cache_slot *slot = &data->slots_array[i];
        slot->is_dirty = 0;
        slot->block_no = 0;
        slot->data_ptr = data->big_data_buffer + (i * block_size_bytes);
    }
    memset(data->used_blocks_bitmap, 0, data->used_slots_bitmap_size);

    cache_layer *cl = memory->allocate(memory, sizeof(cache_layer));
    cl->read = cache_read;
    cl->write = cache_write;
    cl->flush = cache_flush;
    cl->free_memory = cache_free_memory;
    cl->data = data;
    return cl;
}
