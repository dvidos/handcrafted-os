#include "../libc_internal.h"

/**
 * @brief Computes the absolute value of a long long integer.
 *
 * This function returns the absolute value of the long long integer `j`.
 *
 * @param j The long long integer value.
 * @return The absolute value of `j`.
 */
long long llabs(long long j) {
    return (j < 0LL) ? -j : j;
}