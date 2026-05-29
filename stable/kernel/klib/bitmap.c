#include "bitmap.h"
#include "../memory/kheap.h"
#include "string.h"



static bool bitmap_is_used(bitmap_t *bm, uint32_t bit) {
    return (bm->words[bit / 64] >> (bit % 64)) & 1;
}

static bool bitmap_is_free(bitmap_t *bm, uint32_t bit) {
    return ((bm->words[bit / 64] >> (bit % 64)) & 1) == 0;
}

static void bitmap_mark_used(bitmap_t *bm, uint32_t bit) {
    bm->words[bit / 64] |= (1ULL << (bit % 64));
}

static void bitmap_mark_free(bitmap_t *bm, uint32_t bit) {
    bm->words[bit / 64] &= ~(1ULL << (bit % 64));
}

static void bitmap_mark_all_used(bitmap_t *bm) {
    for (uint32_t w = 0; w < bm->words_count; w++) {
        bm->words[w] = 0xFFFFFFFFFFFFFFFFLLU;
    }
}

static void bitmap_mark_all_free(bitmap_t *bm) {
    for (uint32_t w = 0; w < bm->words_count; w++) {
        bm->words[w] = 0;
    }
}

static bool bitmap_find_next_free(bitmap_t *bm, uint32_t *bit) {
    uint32_t start_word = bm->bit_hint / 64;

    for (uint32_t w = start_word; w < bm->words_count; w++) {
        if (bm->words[w] == UINT64_MAX)
            continue;
        
        uint64_t word = bm->words[w];
        uint32_t b = __builtin_ctzll(~word);
        uint32_t index = (w * 64) + b;
        if (index < bm->bits) {
            bm->bit_hint = index + 1;
            *bit = index;
            return true;
        }
    }

    // wraparound
    for (uint32_t w = 0; w < start_word; w++) {
        if (bm->words[w] == UINT64_MAX)
            continue;
        
        uint64_t word = bm->words[w];
        uint32_t b = __builtin_ctzll(~word);
        uint32_t index = (w * 64) + b;
        if (index < bm->bits) {
            bm->bit_hint = index + 1;
            *bit = index;
            return true;
        }
    }

    return false;
}

static void bitmap_destroy(bitmap_t *bm) {
    if (bm) {
        if (bm->words) kfree(bm->words);
        kfree(bm);
    }
}

// -----------------------------------------------------------

static bitmap_ops ops = {
    .is_used        = bitmap_is_used,
    .is_free        = bitmap_is_free,
    .mark_used      = bitmap_mark_used,
    .mark_free      = bitmap_mark_free,
    .mark_all_used  = bitmap_mark_all_used,
    .mark_all_free  = bitmap_mark_all_free,
    .find_next_free = bitmap_find_next_free,
    .destroy        = bitmap_destroy
};

bitmap_t *create_bitmap(uint32_t bits, size_t bytes_count) {
    bitmap_t *bm = kmalloc(sizeof(bitmap_t));

    bm->bits = bits;
    bm->words_count = (bits + 63) / 64;

    // we may allocate more, to simplify load/save
    bm->bytes_count = bytes_count;
    size_t min_bytes = bm->words_count * sizeof(uint64_t);
    bm->words = kmalloc(bytes_count > min_bytes ? bytes_count : min_bytes);

    memset(bm->words, 0, bm->words_count * sizeof(uint64_t));
    bm->bit_hint = 0;

    bm->ops = &ops;
    return bm;
}
