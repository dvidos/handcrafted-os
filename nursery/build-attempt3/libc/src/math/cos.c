#include "../libc_internal.h"

/**
 * @brief Computes the cosine of a floating-point number.
 *
 * This function returns the cosine of `x`, where `x` is given in radians.
 *
 * @param x The value in radians.
 * @return The cosine of `x`.
 *
 * @implNote
 * Similar to `sin`, this requires a transcendental math library. It can be
 * implemented using series expansions or by leveraging `sin(x + PI/2)`.
 * Error handling for very large or NaN inputs is also important.
 */
double cos(double x) {
    // TODO: Implement cos for your operating system.
    (void)x; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return 0.0;
}