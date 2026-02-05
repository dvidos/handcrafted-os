#include "internal.h"

typedef struct block_bitmap block_bitmap;


struct block_bitmap {
    uint32_t blocks_in_device;
    uint32_t block_size;
    uint32_t storage_first_block_no;
    uint32_t size_in_blocks;
    uint32_t size_in_bytes;
    uint32_t next_free_block_hint;
    uint8_t *buffer;

};

static block_bitmap *initialize_block_bitmap(mem_allocator *mem, uint32_t storage_first_block, uint32_t bitmap_size_in_blocks, uint32_t blocks_in_device, uint32_t block_size) {
    // we have to be able to fit the bitmap in the dictated space
    if (ceiling_division(blocks_in_device, 8) > (bitmap_size_in_blocks * block_size))
        return NULL;

    block_bitmap *bitmap = mem->allocate(mem, sizeof(block_bitmap));
    memset(bitmap, 0, sizeof(block_bitmap));

    bitmap->blocks_in_device = blocks_in_device;
    bitmap->block_size = block_size;
    bitmap->storage_first_block_no = storage_first_block;
    bitmap->size_in_blocks = bitmap_size_in_blocks;
    bitmap->size_in_bytes = bitmap_size_in_blocks * block_size; // to cover full blocks, not to cover needed bits only
    bitmap->next_free_block_hint = 0;
    bitmap->buffer = mem->allocate(mem, bitmap->size_in_bytes);
    memset(bitmap->buffer, 0, bitmap->size_in_bytes);

    return bitmap;
}

static int bitmap_load(block_bitmap *bitmap, block_cache *cache) {
    for (int i = 0; i < bitmap->size_in_blocks; i++) {
        int err = bcache_read(cache, bitmap->storage_first_block_no + i, 0, bitmap->buffer + (bitmap->block_size * i), bitmap->block_size);
        if (err != OK) return err;
    }
    return OK;
}

static int bitmap_save(block_bitmap *bitmap, block_cache *cache) {
    for (int i = 0; i < bitmap->size_in_blocks; i++) {
        int err = bcache_write(cache, bitmap->storage_first_block_no + i, 0, bitmap->buffer + (bitmap->block_size * i), bitmap->block_size);
        if (err != OK) return err;
    }
    return OK;
}

static int bitmap_is_block_used(block_bitmap *bitmap, uint32_t block_no) {
    if (block_no >= bitmap->blocks_in_device) return 0;
    return (bitmap->buffer[block_no / 8] & (1 << (block_no % 8))) != 0;
}

static int bitmap_is_block_free(block_bitmap *bitmap, uint32_t block_no) {
    if (block_no >= bitmap->blocks_in_device) return 0;
    return (bitmap->buffer[block_no / 8] & (1 << (block_no % 8))) == 0;
}

static inline void bitmap_mark_block_used(block_bitmap *bitmap, uint32_t block_no) {
    if (block_no >= bitmap->blocks_in_device) return;
    bitmap->buffer[block_no / 8] |= (1 << (block_no % 8));
}

static inline void bitmap_mark_block_free(block_bitmap *bitmap, uint32_t block_no) {
    if (block_no >= bitmap->blocks_in_device) return;
    bitmap->buffer[block_no / 8] &= ~(1 << (block_no % 8));
}

static int bitmap_find_free_block(block_bitmap *bitmap, uint32_t *block_no) {
    int meaningful_bytes = ceiling_division(bitmap->blocks_in_device, 8);
    int byte_no = bitmap->next_free_block_hint / 8;

    for (int attempt = 0; attempt < meaningful_bytes; attempt++) {
        uint8_t byte_value = bitmap->buffer[byte_no];
        if (byte_value == 0xFF) {
            byte_no = (byte_no + 1) % meaningful_bytes;
            continue;
        }

        // we found a byte that is not all used
        for (int bit_no = 0; bit_no < 8; bit_no++) {
            uint32_t candidate = byte_no * 8 + bit_no;

            if (candidate >= bitmap->blocks_in_device)
                break; // last byte may have less than 8 bits, continue from start
            if (byte_value & (1 << bit_no))
                continue; // this bit is allocated

            // we found a free bit, within the block count
            *block_no = candidate;
            bitmap->next_free_block_hint = (candidate + 1) % bitmap->blocks_in_device;
            return OK;
        }

        // maybe the inner bits loop was unsuccesful
        byte_no = (byte_no + 1) % meaningful_bytes;
    }

    return ERR_RESOURCES_EXHAUSTED;
}

static void bitmap_release_memory(block_bitmap *bitmap, mem_allocator *mem) {
    mem->release(mem, bitmap->buffer);
    mem->release(mem, bitmap);
}
static void bitmap_dump_debug_info(block_bitmap *bitmap) {
    char buff[100];
    memset(buff, 0, sizeof(buff));

    for (int i = 0; i < 64; i++)
        buff[i] = bitmap_is_block_used(bitmap, i) ? '1' : '.';
    strcpy(buff + 64, "...");

    printf("Used blocks btmp           1         2         3         4         5         6   \n");
    printf("                 0123456789012345678901234567890123456789012345678901234567890123...\n");
    printf("                 %s\n", buff);
}
