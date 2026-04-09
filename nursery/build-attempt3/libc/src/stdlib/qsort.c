#include "../libc_internal.h"


// Generic swap function
static void swap(char *a, char *b, size_t size) {
    char temp;
    while (size--) {
        temp = *a;
        *a++ = *b;
        *b++ = temp;
    }
}

// Partition function for Quicksort
static char *partition(char *low, char *high, size_t size, int (*compar)(const void *, const void *)) {
    char *pivot = high; // Choose last element as pivot
    char *i = low - size; // Index of smaller element

    for (char *j = low; j < high; j += size) {
        if (compar(j, pivot) <= 0) {
            i += size;
            swap(i, j, size);
        }
    }
    swap(i + size, high, size);
    return i + size;
}

// Recursive Quicksort helper
static void quick_sort_recursive(char *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    if (nmemb <= 1) {
        return;
    }

    char *low = base;
    char *high = base + (nmemb - 1) * size;

    char *pi = partition(low, high, size, compar);

    size_t left_nmemb = (pi - low) / size;
    size_t right_nmemb = (high - pi) / size;

    quick_sort_recursive(low, left_nmemb, size, compar);
    quick_sort_recursive(pi + size, right_nmemb, size, compar);
}

/**
 * @brief Sorts an array using the Quicksort algorithm.
 *
 * This function sorts an array with `nmemb` elements, each of `size` bytes,
 * pointed to by `base`. The `compar` function is used to compare two elements.
 *
 * @param base A pointer to the first element of the array.
 * @param nmemb The number of elements in the array.
 * @param size The size of each element in bytes.
 * @param compar A pointer to a function that compares two elements.
 *               It should return an integer less than, equal to, or greater than zero
 *               if the first argument is considered to be respectively less than,
 *               equal to, or greater than the second.
 */
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    if (base == NULL || nmemb == 0 || size == 0 || compar == NULL) {
        return; // Nothing to sort or invalid arguments
    }
    quick_sort_recursive((char *)base, nmemb, size, compar);
}
