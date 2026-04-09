#include "../libc_internal.h"

/**
 * @brief Performs a binary search on a sorted array.
 *
 * This function searches a sorted array for an element that matches the `key`.
 * The array has `nmemb` elements, each of `size` bytes, pointed to by `base`.
 * The `compar` function is used to compare elements.
 *
 * @param key A pointer to the element to search for.
 * @param base A pointer to the first element of the sorted array.
 * @param nmemb The number of elements in the array.
 * @param size The size of each element in bytes.
 * @param compar A pointer to a function that compares two elements.
 *               It should return an integer less than, equal to, or greater than zero
 *               if the first argument is considered to be respectively less than,
 *               equal to, or greater than the second.
 * @return A pointer to a matching element, or NULL if no match is found.
 */
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    if (key == NULL || base == NULL || nmemb == 0 || size == 0 || compar == NULL) {
        // errno = EINVAL; // Not typically set by bsearch, just return NULL
        return NULL;
    }

    char *arr = (char *)base;
    size_t low = 0;
    size_t high = nmemb - 1;

    while (low <= high) {
        size_t mid_idx = low + (high - low) / 2;
        char *mid_ptr = arr + mid_idx * size;

        int cmp_result = compar(key, mid_ptr);

        if (cmp_result == 0) {
            return mid_ptr; // Found
        } else if (cmp_result < 0) {
            if (mid_idx == 0) { // Avoid underflow
                break;
            }
            high = mid_idx - 1; // Key is in the left half
        } else {
            low = mid_idx + 1; // Key is in the right half
        }
    }

    return NULL; // Not found
}