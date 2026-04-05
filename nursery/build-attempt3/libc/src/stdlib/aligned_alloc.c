#include "libc_internal.h"

/**
 * @brief Allocates aligned memory.
 *
 * This function allocates `size` bytes of memory whose address is a multiple of `alignment`.
 * The `alignment` argument must be a valid alignment supported by the implementation,
 * and must be a power of two.
 *
 * @param alignment The required alignment (must be a power of two).
 * @param size The number of bytes to allocate.
 * @return On success, a pointer to the allocated memory. On failure, a NULL
 *         pointer is returned, and `errno` is set.
 *
 * @implNote
 * This function is part of C11. It's often implemented by allocating a larger
 * block of memory than requested (using `malloc`), finding an aligned address
 * within that block, and storing a pointer to the original `malloc`-ed block
 * just before the aligned address so that `free` can correctly deallocate it.
 */
void *aligned_alloc(size_t alignment, size_t size) {
    // TODO: Implement aligned_alloc for your operating system.
    // This requires careful memory management to ensure alignment.
    (void)alignment; // Suppress unused parameter warning
    (void)size;      // Suppress unused parameter warning
    errno = ENOSYS;  // Function not implemented
    return NULL;
}