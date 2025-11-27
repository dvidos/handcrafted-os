#include "internal.h"

static inline int is_block_used(mounted_data *mt, uint32_t block_no) {
    return (mt->used_blocks_bitmap[block_no / 8] & (1 << (block_no % 8))) != 0;
}

static inline int is_block_free(mounted_data *mt, uint32_t block_no) {
    return (mt->used_blocks_bitmap[block_no / 8] & (1 << (block_no % 8))) == 0;
}

static inline void mark_block_used(mounted_data *mt, uint32_t block_no) {
    mt->used_blocks_bitmap[block_no / 8] |= (1 << (block_no % 8));
}

static inline void mark_block_free(mounted_data *mt, uint32_t block_no) {
    mt->used_blocks_bitmap[block_no / 8] &= ~(1 << (block_no % 8));
}

static int find_next_free_block(mounted_data *mt, uint32_t *block_no) {
    // despite nested, this is quite fast
    int bytes = ceiling_division(mt->superblock->blocks_in_device, 8);
    for (int byte = 0; byte < bytes; byte++) {
        if (mt->used_blocks_bitmap[byte] == 0xFF)
            continue;

        // we found a byte not completely allocated
        uint8_t byte_value = mt->used_blocks_bitmap[byte];
        for (int bit = 0; bit < 8; bit++) {
            if (byte_value & (1 << bit))
                continue;
            
            // we found the free bit
            *block_no = (byte * 8) + bit;
            return OK;
        }
    }

    return ERR_RESOURCES_EXHAUSTED;
}

static void bitmap_dump_debug_info(mounted_data *mt) {
    char bmp[100];
    memset(bmp, 0, sizeof(bmp));

    for (int i = 0; i < 64; i++)
        bmp[i] = is_block_used(mt, i) ? '1' : '.';
    strcpy(bmp + 64, "...");

    printf("Used blocks btmp          1         2         3         4         5         6   \n");
    printf("                 0123456789012345678901234567890123456789012345678901234567890123...\n");
    printf("                 %s\n", bmp);
}