#pragma once
#include <ctypes.h>


typedef struct bitmap {
    uint64_t *words;   // bit storage
    uint32_t bits;     // total number of bits
    uint32_t words_count;
    uint32_t hint;     // next place to start searching
} bitmap_t;


void bitmap_init(bitmap_t *bm, uint32_t bits);
int bitmap_find_next_free(bitmap_t *bm);
void bitmap_destroy(bitmap_t *bm);


static inline bool bitmap_is_used(bitmap_t *bm, uint32_t bit) {
    return (bm->words[bit / 64] >> (bit % 64)) & 1;
}

static inline bool bitmap_is_free(bitmap_t *bm, uint32_t bit) {
    return ((bm->words[bit / 64] >> (bit % 64)) & 1) == 0;
}

static inline void bitmap_mark_used(bitmap_t *bm, uint32_t bit) {
    bm->words[bit / 64] |= (1ULL << (bit % 64));
}

static inline void bitmap_mark_free(bitmap_t *bm, uint32_t bit) {
    bm->words[bit / 64] &= ~(1ULL << (bit % 64));
}
