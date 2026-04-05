#include "libc_internal.h"

/**
 * @brief Initializes a signal set to full, including all signals.
 *
 * This function initializes the signal set pointed to by `set` so that all
 * signals are included in the set.
 *
 * @param set A pointer to the `sigset_t` structure to initialize.
 * @return 0 on success, or -1 on error with `errno` set.
 */
int sigfillset(sigset_t *set) {
    if (set == NULL) {
        errno = EINVAL;
        return -1;
    }
    // Assuming sigset_t is an array of unsigned long.
    // This sets all bits to 1 (all signals included).
    // The actual number of signals may vary, so this might need to be adjusted
    // to include only valid signal numbers if __bits has more than one element
    // and only a subset of signals are supported.
    set->__bits[0] = ~0UL; // All bits set to 1
    return 0;
}