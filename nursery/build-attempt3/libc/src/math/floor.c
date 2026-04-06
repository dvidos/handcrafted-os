#include "../libc_internal.h"

/**
 * @brief Computes the largest integral value not greater than x (floor).
 *
 * This function returns the largest integral value that is not greater than `x`.
 *
 * @param x The double-precision floating-point value.
 * @return The largest integral value not greater than `x`.
 */
double floor(double x) {
    // TODO: Implement floor for your operating system.
    // This typically involves casting to long long, checking for fractional part.
    (void)x; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return 0.0;
}