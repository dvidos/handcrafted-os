#include "qsort.h"

// A basic quicksort for freestanding environments.

static inline void swap_bytes(char *a, char *b, int size) { while (size--) { char tmp = *a; *a++ = *b; *b++ = tmp; } }

static void qsort_internal(char *base, int lo, int hi, int item_size, comparator_func *compare) {
    if (lo >= hi) return;

    int pivot_index = lo + (hi - lo) / 2;
    char *pivot = base + pivot_index * item_size;

    int i = lo, j = hi;
    while (i <= j) {
        // Find left element that should be after pivot
        while (compare(base + i * item_size, pivot) < 0) i++;
        // Find right element that should be before pivot
        while (compare(base + j * item_size, pivot) > 0) j--;

        if (i <= j) {
            swap_bytes(base + i * item_size, base + j * item_size, item_size);
            i++;
            j--;
        }
    }

    if (lo < j)
        qsort_internal(base, lo, j, item_size, compare);
    if (i < hi)
        qsort_internal(base, i, hi, item_size, compare);
}

void qsort(void *base, int item_count, int item_size, comparator_func *compare) {
    if (item_count < 2 || item_size == 0) return;
    qsort_internal((char *)base, 0, (int)item_count - 1, item_size, compare);
}
