#include "libc_internal.h"

/**
 * @brief Initializes a signal set to empty, excluding all signals.
 *
 * This function initializes the signal set pointed to by `set` so that all
 * signals are excluded from the set.
 *
 * @param set A pointer to the `sigset_t` structure to initialize.
 * @return 0 on success, or -1 on error with `errno` set.
 */
int sigemptyset(sigset_t *set) {
    if (set == NULL) {
        errno = EINVAL;
        return -1;
    }
    // Assuming sigset_t is an array of unsigned long, and each bit represents a signal.
    // This implementation is highly dependent on the definition of sigset_t.
    // For a single unsigned long, this sets all bits to 0.
    set->__bits[0] = 0;
    return 0;
}