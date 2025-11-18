#include "internal.h"

static int bitmap_is_block_used(block_bitmap *bb, uint32_t block_no) {
    block_bitmap_data *data = (block_bitmap_data *)bb->data;
    if (block_no >= data->bitmap_size_in_blocks)
        return 0;

    register int byte_no = (block_no / 8);
    register uint8_t mask = 1 << (block_no % 8);
    return (data->buffer[byte_no] & mask) != 0;
}

static int bitmap_is_block_free(block_bitmap *bb, uint32_t block_no) {
    block_bitmap_data *data = (block_bitmap_data *)bb->data;
    if (block_no >= data->bitmap_size_in_blocks)
        return 0;
    
    register int byte_no = (block_no / 8);
    register uint8_t mask = 1 << (block_no % 8);
    return (data->buffer[byte_no] & mask) == 0;
}

static void bitmap_mark_block_used(block_bitmap *bb, uint32_t block_no) {
    block_bitmap_data *data = (block_bitmap_data *)bb->data;
    if (block_no >= data->bitmap_size_in_blocks)
        return;

    register int byte_no = block_no / 8;
    register uint8_t mask = 1 << (block_no % 8);
    data->buffer[byte_no] |= mask;
}

static void bitmap_mark_block_free(block_bitmap *bb, uint32_t block_no) {
    block_bitmap_data *data = (block_bitmap_data *)bb->data;
    if (block_no >= data->bitmap_size_in_blocks)
        return;
    
    register int byte_no = block_no / 8;
    register uint8_t mask = 1 << (block_no % 8);
    data->buffer[byte_no] &= ~mask;
}

static int bitmap_find_a_free_block(block_bitmap *bb, uint32_t *block_no) {
    block_bitmap_data *data = (block_bitmap_data *)bb->data;
    int byte_index = 0;
    int bit_index = 0;

    // first find a non-filled byte
    for (byte_index = 0; byte_index < data->buffer_size; byte_index++) {
        if (data->buffer[byte_index] != 0xFF)
            break;
    }

    // maybe all blocks are used, no more available space
    if (byte_index >= data->buffer_size)
        return ERR_RESOURCES_EXHAUSTED; 

    // find the free bit in this byte
    uint8_t byte_value = data->buffer[byte_index];
    for (bit_index = 0; bit_index < 8; bit_index++) {
        if ((byte_value & (1 << bit_index)) == 0)
            break;
    }

    // should not happen, but...
    if (bit_index >= 8)
        return ERR_RESOURCES_EXHAUSTED;

    if ((byte_index * 8) + bit_index >= data->bitmap_size_in_blocks)
        return ERR_RESOURCES_EXHAUSTED;
    
    *block_no = (byte_index * 8) + bit_index;
    return OK;
}

static void bitmap_release_memory(block_bitmap *bb) {
    block_bitmap_data *data = (block_bitmap_data *)bb->data;
    mem_allocator *m = data->memory;
    
    m->release(m, data->buffer);
    m->release(m, data);
    m->release(m, bb);
}

block_bitmap *new_bitmap(mem_allocator *memory, uint32_t tracked_blocks_count, uint32_t bitmap_blocks_count, uint32_t block_size) {
    block_bitmap_data *d = memory->allocate(memory, sizeof(block_bitmap_data));
    d->tracked_blocks_count = tracked_blocks_count;
    d->bitmap_size_in_blocks = bitmap_blocks_count;
    d->buffer_size = bitmap_blocks_count * block_size;
    d->buffer = memory->allocate(memory, d->buffer_size);

    block_bitmap *b = memory->allocate(memory, sizeof(block_bitmap));
    b->data = d;
    b->is_block_used = bitmap_is_block_used;
    b->is_block_free = bitmap_is_block_free;
    b->mark_block_used = bitmap_mark_block_used;
    b->mark_block_free = bitmap_mark_block_free;
    b->find_a_free_block = bitmap_find_a_free_block;
    b->release_memory = bitmap_release_memory;

    return b;
}

