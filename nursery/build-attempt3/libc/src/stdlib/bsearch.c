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
 *
 * @implNote
 * `bsearch` is a generic searching algorithm that can work with any data type.
 * Its implementation involves:
 * 1. Implementing the binary search algorithm.
 * 2. Using pointer arithmetic to access elements.
 * 3. Repeatedly dividing the search interval in half.
 */
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    // TODO: Implement bsearch for your operating system.
    // This is a complex generic searching algorithm.
    (void)key;    // Suppress unused parameter warning
    (void)base;   // Suppress unused parameter warning
    (void)nmemb;  // Suppress unused parameter warning
    (void)size;   // Suppress unused parameter warning
    (void)compar; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return NULL;
}