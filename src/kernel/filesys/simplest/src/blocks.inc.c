#include "internal.h"


static inline int is_block_used(mounted_data *mt, uint32_t block_no) {
    register int byte_no = (block_no / 8);
    register uint8_t mask = 1 << (block_no % 8);
    return (mt->used_blocks_bitmap[byte_no] & mask) != 0;
}

static inline int is_block_free(mounted_data *mt, uint32_t block_no) {
    register int byte_no = block_no / 8;
    register uint8_t mask = 1 << (block_no % 8);
    return (mt->used_blocks_bitmap[byte_no] & mask) == 0;
}

static inline void mark_block_used(mounted_data *mt, uint32_t block_no) {
    register int byte_no = block_no / 8;
    register uint8_t mask = 1 << (block_no % 8);
    mt->used_blocks_bitmap[byte_no] |= mask;
}

static inline void mark_block_free(mounted_data *mt, uint32_t block_no) {
    register int byte_no = block_no / 8;
    register uint8_t mask = 1 << (block_no % 8);
    mt->used_blocks_bitmap[byte_no] &= ~mask;
}

static int find_a_free_block(mounted_data *mt, uint32_t *block_no) {
    int bytes_in_bitmap = ceiling_division(mt->superblock->blocks_in_device, 8);
    int byte_index = 0;
    int bit_index = 0;

    // first find a non-filled byte
    for (byte_index = 0; byte_index < bytes_in_bitmap; byte_index++) {
        if (mt->used_blocks_bitmap[byte_index] != 0xFF)
            break;
    }

    // maybe all blocks are used, no more available space
    if (byte_index >= bytes_in_bitmap)
        return ERR_RESOURCES_EXHAUSTED; 

    // find the free bit in this byte
    uint8_t byte_value = mt->used_blocks_bitmap[byte_index];
    for (bit_index = 0; bit_index < 8; bit_index++) {
        if ((byte_value & (1 << bit_index)) == 0)
            break;
    }

    // should not happen, but...
    if (bit_index >= 8)
        return ERR_RESOURCES_EXHAUSTED;

    *block_no = (byte_index * 8) + bit_index;
    return OK;
}
