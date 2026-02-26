#pragma once
#include "../include/ctypes.h"

typedef struct bitmap     bitmap_t;
typedef struct bitmap_ops bitmap_ops;

struct bitmap {
    uint64_t *words;       // the actual bit storage
    uint32_t bits;         // total number of bits
    size_t   words_count;
    size_t   bytes_count;  // allocating more space simplifying load/save operations
    uint32_t bit_hint;     // next place to start searching

    bitmap_ops *ops;
};

struct bitmap_ops {
    bool (*is_used)(bitmap_t *bm, uint32_t bit);
    bool (*is_free)(bitmap_t *bm, uint32_t bit);
    void (*mark_used)(bitmap_t *bm, uint32_t bit);
    void (*mark_free)(bitmap_t *bm, uint32_t bit);
    bool (*find_next_free)(bitmap_t *bm, uint32_t *bit);
    void (*destroy)(bitmap_t *bm);
};

bitmap_t *create_bitmap(uint32_t bits, size_t bytes_count);

