#include "../libc_internal.h"

/**
 * @brief Allocates memory for an array and initializes it to zero.
 *
 * This function allocates memory for an array of `nmemb` elements, each of
 * `size` bytes. The allocated memory is initialized to all zero bits.
 *
 * @param nmemb The number of elements in the array.
 * @param size The size of each element.
 * @return On success, a pointer to the allocated and zero-initialized memory.
 *         On failure, a NULL pointer is returned, and `errno` is set.
 *
 * @implNote
 * This can often be implemented as a wrapper around `malloc` followed by `memset`.
 * It needs to handle potential integer overflow if `nmemb * size` exceeds `SIZE_MAX`.
 */
void *calloc(size_t nmemb, size_t size) {
    // TODO: Implement calloc for your operating system.
    // This can often be a wrapper around malloc and memset.
    (void)nmemb; // Suppress unused parameter warning
    (void)size;  // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return NULL;
}