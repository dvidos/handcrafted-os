#include "../libc_internal.h"

int errno;

/**
 * @brief Returns a pointer to the thread-local `errno` variable.
 *
 * In multi-threaded environments, `errno` must be thread-local to prevent race
 * conditions where one thread's error code overwrites another's. This function
 * provides a standard way to access that thread-local storage.
 *
 * @return A pointer to an integer representing the thread-local error number.
 *
 * @implNote
 * A typical implementation would use:
 * 1. `__thread` or `thread_local` keyword (C11) to declare an integer variable
 *    that is unique to each thread.
 * 2. Return the address of this thread-local variable.
 *
 * Example:
 * ```c
 * static __thread int __thread_errno_val;
 * int *__errno_location(void) {
 *     return &__thread_errno_val;
 * }
 * ```
 * The global `errno` macro would then be defined as `*__errno_location()`.
 */
int *__errno_location(void) {
    // TODO: Implement __errno_location for your operating system.
    // This typically involves returning the address of a thread-local errno variable.
    // For a single-threaded environment, it could simply return the address of the global errno.
    static int global_errno; // Placeholder for single-threaded or initial implementation
    return &global_errno;
}