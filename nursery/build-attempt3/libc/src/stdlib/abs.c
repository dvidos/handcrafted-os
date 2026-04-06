#include "../libc_internal.h"

/**
 * @brief Computes the absolute value of an integer.
 *
 * This function returns the absolute value of the integer `j`.
 *
 * @param j The integer value.
 * @return The absolute value of `j`.
 */
int abs(int j) {
    return (j < 0) ? -j : j;
}