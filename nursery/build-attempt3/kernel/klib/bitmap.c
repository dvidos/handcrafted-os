#include "bitmap.h"
#include "../memory/kheap.h"
#include "string.h"


void bitmap_init(bitmap_t *bm, uint32_t bits) {
    bm->bits = bits;
    bm->words_count = (bits + 63) / 64;
    bm->words = kmalloc(bm->words_count * sizeof(uint64_t));
    memset(bm->words, 0, bm->words_count * sizeof(uint64_t));
    bm->hint = 0;
}

int bitmap_find_next_free(bitmap_t *bm) {
    uint32_t start_word = bm->hint / 64;

    for (uint32_t w = start_word; w < bm->words_count; w++) {
        if (bm->words[w] == UINT64_MAX)
            continue;
        
        uint64_t word = bm->words[w];
        uint32_t bit = __builtin_ctzll(~word);
        uint32_t index = (w * 64) + bit;
        if (index < bm->bits) {
            bm->hint = index + 1;
            return index;
        }
    }

    // wraparound
    for (uint32_t w = 0; w < start_word; w++) {
        if (bm->words[w] == UINT64_MAX)
            continue;
        
        uint64_t word = bm->words[w];
        uint32_t bit = __builtin_ctzll(~word);
        uint32_t index = (w * 64) + bit;
        if (index < bm->bits) {
            bm->hint = index + 1;
            return index;
        }
    }

    return -1; // full
}

void bitmap_destroy(bitmap_t *bm) {
    if (bm && bm->words)
        kfree(bm->words);
}
