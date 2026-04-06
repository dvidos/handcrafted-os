#include "../libc_internal.h"

/**
 * @brief Computes the absolute value of a long integer.
 *
 * This function returns the absolute value of the long integer `j`.
 *
 * @param j The long integer value.
 * @return The absolute value of `j`.
 */
long labs(long j) {
    return (j < 0L) ? -j : j;
}